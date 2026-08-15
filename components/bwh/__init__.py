import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import binary_sensor, sensor
from esphome.const import CONF_ICON, CONF_ID, ENTITY_CATEGORY_DIAGNOSTIC

from .. import rka_api  # pylint: disable=relative-beyond-top-level

CODEOWNERS = ["@Druidblack"]
AUTO_LOAD = ["rka_api", "sensor", "binary_sensor"]

CONF_BWH_ID = "bwh_id"
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

bwh_ns = cg.esphome_ns.namespace("bwh")
BWHApi = bwh_ns.class_("BWHApi", cg.Component)
BWHComponent = bwh_ns.class_("BWHComponent", cg.Component)
BWHState = bwh_ns.struct("bwh_state_t")
BWHUpdateTrigger = bwh_ns.class_(
    "BWHUpdateTrigger", automation.Trigger.template(rka_api.obj_const_ref(BWHState))
)

CONFIG_SCHEMA = rka_api.api_schema(BWHApi, trigger_class=BWHUpdateTrigger)


def diagnostic_counter_schema(icon="mdi:counter"):
    return sensor.sensor_schema(
        icon=icon, accuracy_decimals=0, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    )


BWH_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BWH_ID): cv.use_id(BWHApi),
        cv.Optional(CONF_ICON, default=ICON_WATER_BOILER): cv.icon,
        cv.Optional(CONF_COMMUNICATION): binary_sensor.binary_sensor_schema(
            icon="mdi:lan-connect", entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_COMMUNICATION_AGE): sensor.sensor_schema(
            icon="mdi:timer-sand",
            unit_of_measurement="s",
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_RX_FRAMES): diagnostic_counter_schema(
            "mdi:download-network-outline"
        ),
        cv.Optional(CONF_TX_FRAMES): diagnostic_counter_schema(
            "mdi:upload-network-outline"
        ),
        cv.Optional(CONF_CRC_ERRORS): diagnostic_counter_schema(
            "mdi:alert-circle-outline"
        ),
        cv.Optional(CONF_FRAME_TIMEOUTS): diagnostic_counter_schema(
            "mdi:timer-alert-outline"
        ),
        cv.Optional(CONF_INVALID_FRAMES): diagnostic_counter_schema(
            "mdi:message-alert-outline"
        ),
        cv.Optional(CONF_COMMAND_RETRIES): diagnostic_counter_schema("mdi:sync-alert"),
        cv.Optional(CONF_COMMAND_FAILURES): diagnostic_counter_schema(
            "mdi:close-circle-outline"
        ),
        cv.Optional(CONF_QUEUE_OVERFLOWS): diagnostic_counter_schema("mdi:tray-alert"),
    }
).extend(cv.COMPONENT_SCHEMA)


async def new_bwh(config):
    api = await cg.get_variable(config[CONF_BWH_ID])
    var = cg.new_Pvariable(config[CONF_ID], api)
    await cg.register_component(var, config)

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
    await rka_api.new_api(config, BWHState)
