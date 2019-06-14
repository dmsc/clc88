COPY_SRC_ADDR = $00
COPY_DST_ADDR = $02
COPY_SIZE     = $04
COPY_PARAMS   = $06
DISPLAY_START = $08
ATTRIB_START  = $0A
SUBPAL_START  = $0C
VRAM_FREE     = $0E

NMI_VECTOR    = $10
IRQ_VECTOR    = $12
HBLANK_VECTOR = $14
VBLANK_VECTOR = $16
HBLANK_VECTOR_USER = $18
VBLANK_VECTOR_USER = $1A

RAM_TO_VRAM   = $20 ; cpu address
VRAM_TO_RAM   = $22 ; vram address / 2
VRAM_PAGE     = $24 ; vram address page
DLIST         = $30
FRAMECOUNT    = $32

OS_VECTOR     = $7E
OS_VECTORS    = $80

R1            = $C0
R2            = $C1
R3            = $C2
R4            = $C3
R5            = $C4
R6            = $C5
R7            = $C6
R8            = $C7
ROS1          = $C8
ROS2          = $C9
ROS3          = $CA
ROS4          = $CB
ROS5          = $CC
ROS6          = $CD
ROS7          = $CE
ROS8          = $CF

CHRONI_ENABLED = $200
SCREEN_LINES   = $204
SCREEN_SIZE    = $206
ATTRIB_SIZE    = $208
SUBPAL_SIZE    = $20A
SUBPAL_ADDR    = $20C
ATTRIB_DEFAULT = $20E

OS_SET_VIDEO_MODE    = 0
OS_COPY_BLOCK        = 1
OS_COPY_BLOCK_PARAMS = 2
OS_MEM_SET_BYTES     = 3
OS_RAM_TO_VRAM       = 4
OS_VRAM_TO_RAM       = 5
OS_VRAM_SET_BYTES    = 6

OS_CALL  = $F000

VDLIST   = $9000
VCHARSET = $9002
VPALETTE = $9004
VPAGE    = $9006
VCOUNT   = $9007
WSYNC    = $9008
VSTATUS  = $9009
VSPRITES = $900A
VTILESET_SMALL = $900C
VTILESET_BIG   = $900E
VCOLOR0  = $9010
HSCROLL  = $9011
VSCROLL  = $9012

VSTATUS_VSYNC   = $01
VSTATUS_HSYNC   = $02
VSTATUS_EN_INTS = $04
VSTATUS_EN_SPRITES = $08
VSTATUS_ENABLE  = $10

VRAM     = $A000

ST_PROCEED      = $9080
ST_WRITE_ENABLE = $9081
ST_WRITE_DATA   = $9082
ST_READ_ENABLE  = $9083
ST_READ_DATA    = $9084
ST_WRITE_RESET  = $9085 
ST_READ_RESET   = $9086
ST_STATUS       = $9087

ST_CMD_OPEN       = $01
ST_CMD_CLOSE      = $02
ST_CMD_READ_BYTE  = $03
ST_CMD_READ_BLOCK = $04

ST_RET_SUCCESS             = $00
ST_ERR_INVALID_OPERATION   = $80
ST_ERR_FILE_NOT_FOUND      = $81
ST_ERR_EOF                 = $82
ST_ERR_IO                  = $83
ST_ERR_TOO_MANY_OPEN_FILES = $84
ST_ERR_INVALID_FILE        = $85

ST_STATUS_IDLE       = $00
ST_STATUS_PROCESSING = $01
ST_STATUS_DONE       = $FF

ST_MODE_READ  = $00
ST_MODE_WRITE = $01

BOOTADDR = $2000

