import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import (
    CONF_ID,
)

DEPENDENCIES = ["ble_client"]

tesla_ble_ns = cg.esphome_ns.namespace("tesla_ble")
TeslaBle = tesla_ble_ns.class_(
    "TeslaBle", ble_client.BLEClientNode
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(TeslaBle),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ble_client.register_ble_node(var, config)
