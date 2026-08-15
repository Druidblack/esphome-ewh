import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import binary_sensor, sensor, switch, time
from esphome.const import CONF_ICON, CONF_ID, CONF_TIME_ID, ENTITY_CATEGORY_CONFIG, ENTITY_CATEGORY_DIAGNOSTIC
from .. import rka_api  # pylint: disable=relative-beyond-top-level

CODEOWNERS = ["@Druidblack"]
AUTO_LOAD = ["rka_api", "switch", "sensor", "binary_sensor"]

CONF_BST = "bst"
CONF_EWH_ID = "ewh_id"
CONF_ERROR_CODE = "error_code"
CONF_COMMUNICATION = "communication"
CONF_COMMUNICATION_AGE = "communication_age"
CONF_RX_FRAMES = "rx_frames"
CONF_TX_FRAMES = "tx_frames"
CONF_CRC_ERRORS = "crc_errors"
CONF_FRAME_TIMEOUTS = "frame_timeouts"
CONF_INVALID_FRAMES = "invalid_frames"
CONF_COMMAND_RETRIES = "command_retries"
CONF_COMMAND_FAILURES = "command_failures"
CONF_QUEUE_OVERFLOWS = "queue_overflows"
ICON_WATER_BOILER = "mdi:water-boiler"

ewh_ns = cg.esphome_ns.namespace("ewh")
EWHApi = ewh_ns.class_("EWHApi", cg.Component)
EWHComponent = ewh_ns.class_("EWHComponent", cg.Component)
BSTSwitch = EWHComponent.class_("BSTSwitch", switch.Switch)
EWHState = ewh_ns.struct("ewh_state_t")
EWHUpdateTrigger = ewh_ns.class_("EWHUpdateTrigger", automation.Trigger.template(rka_api.obj_const_ref(EWHState)))
CONFIG_SCHEMA = rka_api.api_schema(EWHApi, trigger_class=EWHUpdateTrigger)

def diagnostic_counter_schema(icon="mdi:counter"):
    return sensor.sensor_schema(icon=icon, accuracy_decimals=0, entity_category=ENTITY_CATEGORY_DIAGNOSTIC)

EWH_COMPONENT_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_EWH_ID): cv.use_id(EWHApi),
    cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
    cv.Optional(CONF_ICON, default=ICON_WATER_BOILER): cv.icon,
    cv.Optional(CONF_BST): switch.switch_schema(BSTSwitch, entity_category=ENTITY_CATEGORY_CONFIG, block_inverted=True),
    cv.Optional(CONF_ERROR_CODE): sensor.sensor_schema(icon="mdi:water-boiler-alert", accuracy_decimals=0,
                                                       entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    cv.Optional(CONF_COMMUNICATION): binary_sensor.binary_sensor_schema(icon="mdi:lan-connect",
                                                                        entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    cv.Optional(CONF_COMMUNICATION_AGE): sensor.sensor_schema(icon="mdi:timer-sand", unit_of_measurement="s",
                                                              accuracy_decimals=0,
                                                              entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    cv.Optional(CONF_RX_FRAMES): diagnostic_counter_schema("mdi:download-network-outline"),
    cv.Optional(CONF_TX_FRAMES): diagnostic_counter_schema("mdi:upload-network-outline"),
    cv.Optional(CONF_CRC_ERRORS): diagnostic_counter_schema("mdi:alert-circle-outline"),
    cv.Optional(CONF_FRAME_TIMEOUTS): diagnostic_counter_schema("mdi:timer-alert-outline"),
    cv.Optional(CONF_INVALID_FRAMES): diagnostic_counter_schema("mdi:message-alert-outline"),
    cv.Optional(CONF_COMMAND_RETRIES): diagnostic_counter_schema("mdi:sync-alert"),
    cv.Optional(CONF_COMMAND_FAILURES): diagnostic_counter_schema("mdi:close-circle-outline"),
    cv.Optional(CONF_QUEUE_OVERFLOWS): diagnostic_counter_schema("mdi:tray-alert"),
}).extend(cv.COMPONENT_SCHEMA)

async def new_ewh(config):
    api = await cg.get_variable(config[CONF_EWH_ID])
    var = cg.new_Pvariable(config[CONF_ID], api)
    await cg.register_component(var, config)
    if CONF_BST in config:
        ent = await switch.new_switch(config[CONF_BST], api)
        cg.add(var.set_bst(ent))
    if CONF_ERROR_CODE in config:
        ent = await sensor.new_sensor(config[CONF_ERROR_CODE])
        cg.add(var.set_error_code(ent))
    if CONF_COMMUNICATION in config:
        ent = await binary_sensor.new_binary_sensor(config[CONF_COMMUNICATION])
        cg.add(var.set_communication(ent))
    for key, setter in (
        (CONF_COMMUNICATION_AGE, "set_communication_age"),
        (CONF_RX_FRAMES, "set_rx_frames"),
        (CONF_TX_FRAMES, "set_tx_frames"),
        (CONF_CRC_ERRORS, "set_crc_errors"),
        (CONF_FRAME_TIMEOUTS, "set_frame_timeouts"),
        (CONF_INVALID_FRAMES, "set_invalid_frames"),
        (CONF_COMMAND_RETRIES, "set_command_retries"),
        (CONF_COMMAND_FAILURES, "set_command_failures"),
        (CONF_QUEUE_OVERFLOWS, "set_queue_overflows"),
    ):
        if key in config:
            ent = await sensor.new_sensor(config[key])
            cg.add(getattr(var, setter)(ent))
    return var

async def to_code(config):
    var = await rka_api.new_api(config, EWHState)
    if CONF_TIME_ID in config:
        time_ = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time_id(time_))
