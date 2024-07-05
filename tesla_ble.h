#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/ble_client/ble_client.h"

namespace TeslaBLE {
    class Client;
}

namespace esphome {
namespace tesla_ble {

    struct TeslaBleRestoreState {
        uint8_t private_key[300]{0};
        size_t private_key_length{0};
        u_int32_t counter{0};
    };

    enum TeslaBleStatus {
        DISCONNECTED = 0,
        BLE_CONNECTING = 10,
        BLE_CONNECTED = 20,
        BLE_READY = 30,
        VCSEC_REQUESTING_WHITELIST_INFO = 35,
        VCSEC_RECEIVED_WHITELIST_INFO = 36,
        VCSEC_REQUEST_WHITELIST = 37,
        VCSEC_RECEIVED_WHITELIST = 38,
        VCSEC_REQUESTING_EPHEMERAL_KEY = 40,
        VCSEC_RECEIVED_EPHEMERAL_KEY = 50,
        VCSEC_AUTHENTICATING = 60,
        VCSEC_AUTHENTICATED = 70,
    };

    class TeslaBle : public ble_client::BLEClientNode {
        public:
            TeslaBle();
            
            void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                    esp_ble_gattc_cb_param_t *param) override;

            virtual void loop();

            bool isConnected() const;

            esp_err_t write(uint8_t* data, size_t length);

            bool savePreferences();

            bool loadPreferences();

            bool pairKey();

            bool authenticate();

            bool sendAction(int action);

            bool lock();

            bool unlock();

            bool requestInformation(int info);

            bool requestInformationUnsigned(int info);

            bool requestEphemeralKey();

            bool requestWhitelistInfo();

            int getCounter();

        protected:
            TeslaBleStatus m_status{DISCONNECTED};
            ESPPreferenceObject m_pref;
            TeslaBLE::Client* m_pClient{nullptr};
            ble_client::BLECharacteristic* m_pReadChar{nullptr};
            ble_client::BLECharacteristic* m_pWriteChar{nullptr};
    };
}
}
