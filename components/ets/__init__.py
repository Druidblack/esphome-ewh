import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import binary_sensor, sensor
from esphome.const import (
    CONF_ICON,
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

from .. import rka_api  # pylint: disable=relative-beyond-top-level

CODEOWNERS = ["@Druidblack"]
AUTO_LOAD = ["rka_api", "sensor", "binary_sensor"]

CONF_ETS_ID = "ets_id"
CONF_FLOOR_TEMP = "floor_temperature"
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

ICON_THERMOSTAT = "mdi:thermostat"

ets_ns = cg.esphome_ns.namespace("ets")
ETSApi = ets_ns.class_("ETSApi", cg.Component)
ETSComponent = ets_ns.class_("ETSComponent", cg.PollingComponent)
ETSState = ets_ns.struct("ets_state_t")
ETSUpdateTrigger = ets_ns.class_(
    "ETSUpdateTrigger", automation.Trigger.template(rka_api.obj_const_ref(ETSState))
)

CONFIG_SCHEMA = rka_api.api_schema(ETSApi, trigger_class=ETSUpdateTrigger)


def diagnostic_counter_schema(icon="mdi:counter"):
    return sensor.sensor_schema(
        icon=icon, accuracy_decimals=0, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    )


ETS_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ETS_ID): cv.use_id(ETSApi),
        cv.Optional(CONF_ICON, default=ICON_THERMOSTAT): cv.icon,
        cv.Optional(CONF_FLOOR_TEMP): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:heated-floor",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
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
).extend(cv.polling_component_schema("30s"))


async def new_ets(config):
    api = await cg.get_variable(config[CONF_ETS_ID])
    var = cg.new_Pvariable(config[CONF_ID], api)
    await cg.register_component(var, config)

    if CONF_FLOOR_TEMP in config:
        sens = await sensor.new_sensor(config[CONF_FLOOR_TEMP])
        cg.add(var.set_floor_temp(sens))

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
    await rka_api.new_api(config, ETSState)
