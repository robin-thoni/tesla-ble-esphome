#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"

namespace TeslaBLE {
    class Client;
}

namespace esphome {
namespace tesla_ble {
    class TeslaBle : public ble_client::BLEClientNode {
        public:
            TeslaBle();
            
            void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                    esp_ble_gattc_cb_param_t *param) override;

            void test();

        protected:
            TeslaBLE::Client* m_pClient;
    };
}
}
