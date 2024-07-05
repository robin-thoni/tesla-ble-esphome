#include "tesla_ble.h"
#include "esphome/core/log.h"
#include "client.h"

static const char *const TAG = "TESLA_BLE";

namespace esphome {
namespace tesla_ble {

// Random 32bit value; If this changes existing restore preferences are invalidated
static const uint32_t RESTORE_STATE_VERSION = 0xe22755bf;

TeslaBle::TeslaBle()
    : m_pClient(new TeslaBLE::Client{})
{
}

void TeslaBle::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                    esp_ble_gattc_cb_param_t *param) {
    switch (event) {
        case ESP_GATTC_CONNECT_EVT: {
            m_status = BLE_CONNECTING;
            m_pref = global_preferences->make_preference<TeslaBleRestoreState>(parent_->get_address() ^ RESTORE_STATE_VERSION);
            if (!loadPreferences()) {
                m_pClient->CreatePrivateKey();
                m_pClient->SetCounter(1);
                savePreferences();
            }
        }
        case ESP_GATTC_OPEN_EVT: {
            if (param->open.status != ESP_GATT_OK) {
                m_status = BLE_CONNECTED;
            } else {
                m_status = DISCONNECTED;
            }
        }
        break;
        case ESP_GATTC_SEARCH_CMPL_EVT: {
            m_pReadChar = this->parent()->get_characteristic(esp32_ble::ESPBTUUID::from_raw("00000211-b2d1-43f0-9b88-960cebf8b91e"), esp32_ble::ESPBTUUID::from_raw("00000213-b2d1-43f0-9b88-960cebf8b91e"));
            if (!m_pReadChar) {
                ESP_LOGE(TAG, "Failed to get read_char");
                parent()->disconnect();
                return;
            }
            auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(),
                                                            this->parent()->get_remote_bda(), m_pReadChar->handle);
            if (status) {
                ESP_LOGE(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
                parent()->disconnect();
                return;
            }

            m_pWriteChar = this->parent()->get_characteristic(esp32_ble::ESPBTUUID::from_raw("00000211-b2d1-43f0-9b88-960cebf8b91e"), esp32_ble::ESPBTUUID::from_raw("00000212-b2d1-43f0-9b88-960cebf8b91e"));
            if (!m_pWriteChar) {
                ESP_LOGE(TAG, "Failed to get write_char");
                parent()->disconnect();
                return;
            }
            m_status = BLE_READY;
        }
        break;
        case ESP_GATTC_NOTIFY_EVT: {
            ESP_LOGD(TAG, "ESP_GATTC_NOTIFY_EVT, value_len=%d", param->notify.value_len);
            VCSEC_FromVCSECMessage message_o = VCSEC_FromVCSECMessage_init_zero;
            int return_code = m_pClient->ParseFromVCSECMessage(param->notify.value, param->notify.value_len, &message_o);
            if (return_code != 0) {
                ESP_LOGE(TAG, "Failed to parse incomming message: %s", format_hex_pretty(param->notify.value, param->notify.value_len).c_str());
                return;
            }
            ESP_LOGD(TAG, "Got message: %i", message_o.which_sub_message);

            switch (message_o.which_sub_message) {
                case VCSEC_FromVCSECMessage_authenticationRequest_tag: {
                    const auto& authenticationRequest = message_o.sub_message.authenticationRequest;
                    ESP_LOGD(TAG, "Received authenticationRequest");
                }
                break;
                case VCSEC_FromVCSECMessage_sessionInfo_tag: {
                    const auto& sessionInfo = message_o.sub_message.sessionInfo;
                    if (m_status == VCSEC_REQUESTING_EPHEMERAL_KEY) {
                        ESP_LOGI(TAG, "Received ephemeral key");

                        int result_code =
                            m_pClient->LoadTeslaKey(sessionInfo.publicKey.bytes,
                                                sessionInfo.publicKey.size);

                        if (result_code != 0) {
                            ESP_LOGE(TAG, "Failed load tesla key");
                            parent()->disconnect();
                            return;
                        }
                        m_status = VCSEC_RECEIVED_EPHEMERAL_KEY;
                    } else {
                        ESP_LOGW(TAG, "Received unsolicited ephemeral key"); // TODO might be acceptable? Ignore for now
                    }
                }
                break;
                case VCSEC_FromVCSECMessage_whitelistInfo_tag: {
                    const auto& whitelistInfo = message_o.sub_message.whitelistInfo;
                    ESP_LOGI(TAG, "Received Whitelist Info numberOfEntries=%d whitelistEntries_count=%lu", whitelistInfo.numberOfEntries, whitelistInfo.whitelistEntries_count);
                    ESP_LOGI(TAG, "  Our Key: %s", format_hex_pretty(m_pClient->GetKeyId(), 4).c_str());
                    auto ourKeyWhitelisted = false;
                    for (int i = 0; i < whitelistInfo.whitelistEntries_count; ++i) {
                        auto isOurKey = memcmp(m_pClient->GetKeyId(), whitelistInfo.whitelistEntries[i].publicKeySHA1, 4) == 0;
                        if (isOurKey) {
                            ourKeyWhitelisted = true;
                        }
                        ESP_LOGI(TAG, "  Key %i: %s%s", i, format_hex_pretty(whitelistInfo.whitelistEntries[i].publicKeySHA1, 4).c_str(), isOurKey ? " *" : "");
                    }
                    if (ourKeyWhitelisted) {
                        m_status = VCSEC_RECEIVED_WHITELIST;
                    } else {
                        m_status = VCSEC_RECEIVED_WHITELIST_INFO;
                    }
                }
                break;
                case VCSEC_FromVCSECMessage_commandStatus_tag: {
                    const auto& commandStatus = message_o.sub_message.commandStatus;
                    ESP_LOGI(TAG, "Received command status: %lu", commandStatus.operationStatus);
                    switch (commandStatus.which_sub_message) {
                        case VCSEC_CommandStatus_operationStatus_tag: {
                            const auto& operationStatus = commandStatus.operationStatus;
                            if (m_status == VCSEC_AUTHENTICATING) {
                                if (commandStatus.operationStatus == VCSEC_OperationStatus_E_OPERATIONSTATUS_OK) {
                                    m_status = VCSEC_AUTHENTICATED;
                                } else if (commandStatus.operationStatus == VCSEC_OperationStatus_E_OPERATIONSTATUS_ERROR) {
                                    ESP_LOGE(TAG, "Failed to authenticate");
                                    parent()->disconnect();
                                    return;
                                }
                            }
                        }
                        break;
                        case VCSEC_CommandStatus_signedMessageStatus_tag: {
                            const auto& signedMessageStatus = commandStatus.sub_message.signedMessageStatus;
                            ESP_LOGI(TAG, "Received new counter from the car: %lu", signedMessageStatus.counter);
                            m_pClient->SetCounter(signedMessageStatus.counter + 1);
                            savePreferences();
                        }
                        break;
                        case VCSEC_CommandStatus_whitelistOperationStatus_tag: {
                            ESP_LOGI(TAG, "Received Whitelist Info in commandStatus");
                        }
                        break;
                    }
                }
                break;
                case VCSEC_FromVCSECMessage_vehicleStatus_tag: {
                    ESP_LOGI(TAG, "Received status message:");
                    ESP_LOGI(TAG, "Car is \"%s\"",
                            message_o.sub_message.vehicleStatus.vehicleLockState ? "locked"
                                                                                : "unlocked");
                    ESP_LOGI(TAG, "Car is \"%s\"",
                            message_o.sub_message.vehicleStatus.vehicleSleepStatus
                                ? "awake"
                                : "sleeping");
                    ESP_LOGI(TAG, "Charge port is \"%s\"",
                            message_o.sub_message.vehicleStatus.closureStatuses.chargePort
                                ? "open"
                                : "closed");
                    ESP_LOGI(TAG, "Front driver door is \"%s\"",
                            message_o.sub_message.vehicleStatus.closureStatuses.frontDriverDoor
                                ? "open"
                                : "closed");
                    // ESP_LOGI(
                    //     TAG, "Front passenger door is \"%s\"",
                    //     message_o.sub_message.vehicleStatus.closureStatuses.frontPassengerDoor
                    //         ? "open"
                    //         : "closed");
                    // ESP_LOGI(TAG, "Rear driver door is \"%s\"",
                    //         message_o.sub_message.vehicleStatus.closureStatuses.rearDriverDoor
                    //             ? "open"
                    //             : "closed");
                    // ESP_LOGI(
                    //     TAG, "Rear passenger door is \"%s\"",
                    //     message_o.sub_message.vehicleStatus.closureStatuses.rearPassengerDoor
                    //         ? "open"
                    //         : "closed");
                    // ESP_LOGI(TAG, "Front trunk is \"%s\"",
                    //         message_o.sub_message.vehicleStatus.closureStatuses.frontTrunk
                    //             ? "open"
                    //             : "closed");
                    // ESP_LOGI(TAG, "Rear trunk is \"%s\"",
                    //         message_o.sub_message.vehicleStatus.closureStatuses.rearTrunk
                    //             ? "open"
                    //             : "closed");
                }
                break;
            }
        }
        break;
        case ESP_GATTC_DISCONNECT_EVT: {
            m_pReadChar = nullptr;
            m_pWriteChar = nullptr;
            m_status = DISCONNECTED;
        }
        break;
    }
}

void TeslaBle::loop() {
    switch (m_status) {
        case BLE_READY: {
            m_status = VCSEC_REQUESTING_WHITELIST_INFO;
            requestWhitelistInfo();
        }
        break;
        case VCSEC_RECEIVED_WHITELIST_INFO: {
            m_status = VCSEC_REQUEST_WHITELIST;
            pairKey();
        }
        break;
        case VCSEC_REQUEST_WHITELIST: {
            static uint32_t lastTs = 0;
            if (lastTs == 0) {
                lastTs = millis();
            } else if (millis() - lastTs > 1000) {
                lastTs = millis();
                m_status = VCSEC_REQUESTING_WHITELIST_INFO;
                requestWhitelistInfo();
            }
        }
        break;
        case VCSEC_RECEIVED_WHITELIST: {
            m_status = VCSEC_REQUESTING_EPHEMERAL_KEY;
            requestEphemeralKey();
        }
        break;
        case VCSEC_RECEIVED_EPHEMERAL_KEY: {
            m_status = VCSEC_AUTHENTICATING;
            authenticate();
        }
        break;
    }
}

bool TeslaBle::isConnected() const {
    return m_pReadChar && m_pWriteChar;
}

esp_err_t TeslaBle::write(uint8_t* data, size_t length) {
    if (!isConnected()) {
        ESP_LOGW(TAG, "Trying to write %i bytes, while not connected", length);
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "Writing %i bytes", length);
    return esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), m_pWriteChar->handle, length, data, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
}

bool TeslaBle::savePreferences() {
    TeslaBleRestoreState saved{};
    m_pClient->GetPrivateKey(saved.private_key, sizeof(saved.private_key), &saved.private_key_length);
    saved.counter = m_pClient->GetCounter();
    return m_pref.save(&saved);
}

bool TeslaBle::loadPreferences() {
    TeslaBleRestoreState recovered{};
    if (m_pref.load(&recovered)) {
        m_pClient->LoadPrivateKey(recovered.private_key, recovered.private_key_length);
        m_pClient->SetCounter(recovered.counter);
        return true;
    }
    return false;
}

bool TeslaBle::pairKey() {
    unsigned char message_buffer[200];
    size_t message_buffer_length = 0;
    m_pClient->BuildWhiteListMessage(message_buffer, &message_buffer_length);

    auto chr_status = write(message_buffer, message_buffer_length);
    if (chr_status) {
        ESP_LOGE(TAG, "Error sending the white list message, status=%d", chr_status);
        return false;
    }
    ESP_LOGI(TAG, "Tab your card on the reader, even if the car doesn't display any pairing request");
    return true;
}

bool TeslaBle::authenticate() {
    unsigned char message_buffer[200];
    size_t message_buffer_length = 0;
    m_pClient->BuildAuthenticationResponse(VCSEC_AuthenticationLevel_E_AUTHENTICATION_LEVEL_UNLOCK, message_buffer, &message_buffer_length);

    auto chr_status = write(message_buffer, message_buffer_length);
    if (chr_status) {
        ESP_LOGE(TAG, "Error sending the authenticate message, status=%d", chr_status);
        return false;
    }
    return true;
}

bool TeslaBle::sendAction(int action) {
    unsigned char message_buffer[200];
    size_t message_buffer_length = 0;
    m_pClient->BuildActionMessage((VCSEC_RKEAction_E)action, message_buffer, &message_buffer_length);

    auto chr_status = write(message_buffer, message_buffer_length);
    if (chr_status) {
        ESP_LOGE(TAG, "Error sending the action message %i, status=%d", action, chr_status);
        return false;
    }
    return true;
}

bool TeslaBle::lock() {
    return sendAction(VCSEC_RKEAction_E_RKE_ACTION_LOCK);
}

bool TeslaBle::unlock() {
    return sendAction(VCSEC_RKEAction_E_RKE_ACTION_UNLOCK);
}

bool TeslaBle::requestInformation(int info) {
    unsigned char message_buffer[200];
    size_t message_buffer_length = 0;
    m_pClient->BuildInformationRequestMessage((VCSEC_InformationRequestType)info, message_buffer, &message_buffer_length);

    auto chr_status = write(message_buffer, message_buffer_length);
    if (chr_status) {
        ESP_LOGE(TAG, "Error sending the requestInfo message %i, status=%d", info, chr_status);
        return false;
    }
    return true;
}

bool TeslaBle::requestInformationUnsigned(int info) {
    unsigned char message_buffer[200];
    size_t message_buffer_length = 0;
    m_pClient->BuildInformationRequestUnsignedMessage((VCSEC_InformationRequestType)info, message_buffer, &message_buffer_length);

    auto chr_status = write(message_buffer, message_buffer_length);
    if (chr_status) {
        ESP_LOGE(TAG, "Error sending the requestInfo message %i, status=%d", info, chr_status);
        return false;
    }
    return true;
}

bool TeslaBle::requestEphemeralKey() {
    return requestInformationUnsigned(VCSEC_InformationRequestType_INFORMATION_REQUEST_TYPE_GET_EPHEMERAL_PUBLIC_KEY);
}

bool TeslaBle::requestWhitelistInfo() {
    return requestInformationUnsigned(VCSEC_InformationRequestType_INFORMATION_REQUEST_TYPE_GET_WHITELIST_INFO);
}

int TeslaBle::getCounter() {
    return m_pClient->GetCounter();
}

}
}
