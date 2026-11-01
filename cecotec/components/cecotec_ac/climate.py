import esphome.codegen as cg
from esphome.components import climate, remote_base, sensor
import esphome.config_validation as cv
from esphome.const import CONF_HUMIDITY_SENSOR, CONF_SENSOR

DEPENDENCIES = ["remote_transmitter"]
AUTO_LOAD = ["remote_base", "sensor"]

cecotec_ac_ns = cg.esphome_ns.namespace("cecotec_ac")
CecotecClimate = cecotec_ac_ns.class_(
    "CecotecClimate",
    climate.Climate,
    cg.Component,
    remote_base.RemoteTransmittable,
)

CONFIG_SCHEMA = (
    climate.climate_schema(CecotecClimate)
    .extend(
        {
            cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
            # Dry mode is humidity-controlled, so it needs a humidity reading.
            cv.Required(CONF_HUMIDITY_SENSOR): cv.use_id(sensor.Sensor),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(remote_base.REMOTE_TRANSMITTABLE_SCHEMA)
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await remote_base.register_transmittable(var, config)
    if sensor_id := config.get(CONF_SENSOR):
        cg.add(var.set_sensor(await cg.get_variable(sensor_id)))
    cg.add(
        var.set_humidity_sensor(await cg.get_variable(config[CONF_HUMIDITY_SENSOR]))
    )
