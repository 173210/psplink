	.set noreorder

#include "pspstub.s"

	STUB_START "USBHostFS",0x40090000,0x00050005
	STUB_FUNC  0x9CDF16EA,usb_read_data
	STUB_FUNC  0xF4C6E005,usb_write_data
	STUB_FUNC  0xF4AF883C,usb_read_async_data
	STUB_FUNC  0x416CC77E,usb_write_async_data
	STUB_FUNC  0x93DEAC83,usb_async_flush
	STUB_END
