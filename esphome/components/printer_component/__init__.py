import esphome.codegen as cg
import esphome.config_validation as cv

# from esphome.components import uart
from esphome.const import CONF_ID

# DEPENDENCIES = ["uart"]

thermalprinter_ns = cg.esphome_ns.namespace("thermalprinter")
PrinterComponent = thermalprinter_ns.class_("Epson", cg.Component)  # , uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(PrinterComponent)}).extend(cv.COMPONENT_SCHEMA)
    # .extend(uart.UART_DEVICE_SCHEMA)
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    # yield uart.register_uart_device(var, config)
