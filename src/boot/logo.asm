section .rodata
global logo_data
global logo_width
global logo_height

logo_width  dd 128
logo_height dd 128

logo_data:
    incbin "../boot/logo.raw"
