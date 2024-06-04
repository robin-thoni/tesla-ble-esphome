#include "tesla_ble.h"
#include "esphome/core/log.h"
#include "client.h"

static const char *const TAG = "TESLA_BLE";

namespace esphome {
namespace tesla_ble {

TeslaBle::TeslaBle()
    : m_pClient(new TeslaBLE::Client{})
{
    m_pClient->CreatePrivateKey();
}

void TeslaBle::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                    esp_ble_gattc_cb_param_t *param) {
    if (event == ESP_GATTC_OPEN_EVT) {
        if (param->open.status == ESP_GATT_OK) {
            ESP_LOGD(TAG, "[%s] Connected successfully!", this->parent()->address_str().c_str());
        }
    }
    else if (event == ESP_GATTC_SEARCH_CMPL_EVT) {
        auto *read_char = this->parent()->get_characteristic(esp32_ble::ESPBTUUID::from_raw("00000211-b2d1-43f0-9b88-960cebf8b91e"), esp32_ble::ESPBTUUID::from_raw("00000213-b2d1-43f0-9b88-960cebf8b91e"));
        if (!read_char) {
            ESP_LOGW(TAG, "Failed to get read_char");
            return;
        }
        auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(),
                                                        this->parent()->get_remote_bda(), read_char->handle);
        if (status) {
            ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
        }
        auto *write_char = this->parent()->get_characteristic(esp32_ble::ESPBTUUID::from_raw("00000211-b2d1-43f0-9b88-960cebf8b91e"), esp32_ble::ESPBTUUID::from_raw("00000212-b2d1-43f0-9b88-960cebf8b91e"));
        if (!write_char) {
            ESP_LOGW(TAG, "Failed to get write_char");
            return;
        }
    }
    else if (event == ESP_GATTC_NOTIFY_EVT) {
        ESP_LOGD(TAG, "ESP_GATTC_NOTIFY_EVT, value_len=%d", param->notify.value_len);
        VCSEC_FromVCSECMessage message_o = VCSEC_FromVCSECMessage_init_zero;
        int return_code = m_pClient->ParseFromVCSECMessage(param->notify.value, param->notify.value_len, &message_o);
        if (return_code != 0) {
            ESP_LOGE(TAG, "Failed to parse incomming message\n");
            return;
        }
    }
    else if (event == ESP_GATTC_DISCONNECT_EVT) {
        ESP_LOGD(TAG, "[%s] Disconnected!", this->parent()->address_str().c_str());
    }
}

void TeslaBle::test() {
    unsigned char private_key_buffer[300];
    size_t private_key_length = 0;
    m_pClient->GetPrivateKey(private_key_buffer, sizeof(private_key_buffer),&private_key_length);
    ESP_LOGD(TAG, "Private key: %s\n", private_key_buffer);

    unsigned char message_buffer[200];
    size_t message_buffer_length = 0;
    m_pClient->BuildWhiteListMessage(message_buffer, &message_buffer_length);
    auto *write_char = this->parent()->get_characteristic(esp32_ble::ESPBTUUID::from_raw("00000211-b2d1-43f0-9b88-960cebf8b91e"), esp32_ble::ESPBTUUID::from_raw("00000212-b2d1-43f0-9b88-960cebf8b91e"));
    if (!write_char) {
        ESP_LOGW(TAG, "Failed to get write_char");
        return;
    }
    
    auto chr_status = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), write_char->handle, message_buffer_length, message_buffer, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
    if (chr_status) {
        ESP_LOGW(TAG, "Error sending the white list message, status=%d", chr_status);
        return ;
    }
}

}
}
