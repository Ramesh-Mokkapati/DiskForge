// resource.h - Resource identifiers
#pragma once

// ---- Application frame (NewTest-style ribbon / docking framework) ----
// IDR_MAINFRAME (128) is the frame's icon, title string and menu.
#define IDR_THEME_MENU            200
#define IDR_POPUP_BROWSER         201
#define IDR_OUTPUT_POPUP          202
#define IDR_PROPERTIES            203   // Properties pane toolbar (+ bitmap)
#define IDB_PROPERTIES_HC         204
#define IDB_PAGES                 205
#define IDB_PAGES_HC              206
#define IDB_PAGES_SMALL           207
#define IDB_PAGES_SMALL_HC        208
#define IDB_INFO                  209
#define IDB_MAIN                  210
#define IDI_OUTPUT_WND            211
#define IDI_OUTPUT_WND_HC         212
#define IDI_PROPERTIES_WND        213
#define IDI_PROPERTIES_WND_HC     214

#define IDS_OUTPUT_WND            300
#define IDS_PROPERTIES_WND        301
#define IDS_HEX_WND               302
#define IDS_SHORTCUTS             303
#define IDS_OUTLOOKBAR            304
#define IDS_PAGE_TOOLS            305
#define IDS_PAGE_RECOVERY         306
#define IDS_PAGE_BOOT             307
#define IDS_CAPTION_TEXT          308
#define IDS_CAPTION_BUTTON        309
#define IDS_CAPTION_BUTTON_TIP    310
#define IDS_CAPTION_IMAGE_TIP     311
#define IDS_CAPTION_IMAGE_TEXT    312
#define IDS_LOG_TAB               313
#define IDS_STATUS_PANE1          314
#define IDS_STATUS_PANE2          315

// ---- Panes and status bar ----
#define ID_VIEW_OUTPUTWND         32800
#define ID_VIEW_PROPERTIESWND     32801
#define ID_VIEW_HEXWND            32802
#define ID_VIEW_NAVIGATION        32803
#define ID_VIEW_CAPTION_BAR       32804
#define ID_STATUSBAR_PANE1        32805
#define ID_STATUSBAR_PANE2        32806
#define ID_EXPAND_ALL             32807
#define ID_SORTPROPERTIES         32808
#define ID_TOOLS_OPTIONS          32809

// Application look (keep contiguous - handled as a range)
#define ID_VIEW_APPLOOK_WIN_2000        32820
#define ID_VIEW_APPLOOK_OFF_XP          32821
#define ID_VIEW_APPLOOK_WIN_XP          32822
#define ID_VIEW_APPLOOK_OFF_2003        32823
#define ID_VIEW_APPLOOK_VS_2005         32824
#define ID_VIEW_APPLOOK_VS_2008         32825
#define ID_VIEW_APPLOOK_OFF_2007_BLUE   32826
#define ID_VIEW_APPLOOK_OFF_2007_BLACK  32827
#define ID_VIEW_APPLOOK_OFF_2007_SILVER 32828
#define ID_VIEW_APPLOOK_OFF_2007_AQUA   32829
#define ID_VIEW_APPLOOK_WINDOWS_7       32830

// ---- System Browser ----
#define ID_DISK_REFRESH           32840
#define ID_DISK_OPEN_IMAGE        32848
#define ID_BROWSE_SHOW_DELETED    32841
#define ID_BROWSE_DELETED_ONLY    32842
#define ID_BROWSE_RECOVER         32843
#define ID_BROWSE_SHOW_META       32844   // hex view: directory entry / MFT record
#define ID_BROWSE_SHOW_DATA       32845   // hex view: first data sector
#define ID_BROWSE_OPEN            32846   // open folder / partition in the tree
#define ID_BROWSE_UP              32847

// ---- Sector (hex) navigation ----
#define ID_SECTOR_FIRST           32860
#define ID_SECTOR_PREV            32861
#define ID_SECTOR_NEXT            32862
#define ID_SECTOR_LAST            32863
#define ID_SECTOR_EDIT            32864   // ribbon edit box; Enter = go
#define ID_SECTOR_GOTO            32865

// ---- Disk tools (each opens its existing dialog) ----
#define ID_TOOL_PARTITIONS        32880
#define ID_TOOL_DISKCOPY          32881
#define ID_TOOL_BADSECTOR         32882
#define ID_TOOL_SMART             32883
#define ID_TOOL_MBRGPT            32884
#define ID_TOOL_PARTEDIT          32885
#define ID_TOOL_WIPE              32886
#define ID_TOOL_EFIBOOT           32887
#define ID_TOOL_LOSTPART          32888
#define ID_TOOL_FILERECOVERY      32889

// Dialog template IDs
#define IDR_MAINFRAME       128
#define IDD_ABOUTBOX        100
#define IDD_PARTITIONS      101
#define IDD_CONFIRM_TEXT    102
#define IDD_DISKCOPY        103
#define IDD_BADSECTORSCAN   104
#define IDD_SMARTHEALTH     105
#define IDD_MBRGPTCONVERT   106
#define IDD_PARTITIONEDIT   107
#define IDD_WIPEDISK        108
#define IDD_EFIBOOT         109
#define IDD_LOSTPARTITION   110
#define IDD_FILERECOVERY    111
#define IDD_EXTENDPARTITION    113

// ---- IDD_PARTITIONS (CPartitionDialog) ----
#define IDC_PART_LIST            2001
#define IDC_PART_STATIC_SUMMARY  2002
#define IDC_PART_BTN_GOTO        2003
#define IDC_PART_BTN_CLOSE       2004

// ---- IDD_CONFIRM_TEXT (CConfirmTextDialog) ----
#define IDC_CONFIRM_STATIC_MSG   3001
#define IDC_CONFIRM_STATIC_HINT  3002
#define IDC_CONFIRM_EDIT         3003
#define IDC_CONFIRM_BTN_OK       3004
#define IDC_CONFIRM_BTN_CANCEL   3005

// ---- IDD_DISKCOPY (CDiskCopyDialog) ----
#define IDC_DC_COMBO_SRC             4001
#define IDC_DC_RADIO_SRC_WHOLE       4002
#define IDC_DC_RADIO_SRC_PART        4003
#define IDC_DC_BTN_CHOOSE_PART       4004
#define IDC_DC_STATIC_PART           4005
#define IDC_DC_RADIO_DST_DISK        4006
#define IDC_DC_RADIO_DST_FILE        4007
#define IDC_DC_COMBO_DST             4008
#define IDC_DC_EDIT_DST_FILE         4009
#define IDC_DC_BTN_BROWSE            4010
#define IDC_DC_BTN_REFRESH           4011
#define IDC_DC_STATIC_SUMMARY        4012
#define IDC_DC_PROGRESS              4013
#define IDC_DC_STATIC_STATUS         4014
#define IDC_DC_BTN_START             4015
#define IDC_DC_BTN_CANCEL            4016
#define IDC_DC_BTN_CLOSE             4017
#define IDC_DC_RADIO_SRC_TYPE_DRIVE  4018
#define IDC_DC_RADIO_SRC_TYPE_FILE   4019
#define IDC_DC_EDIT_SRC_FILE         4020
#define IDC_DC_BTN_BROWSE_SRC        4021
#define IDC_DC_RADIO_DST_WHOLE       4022
#define IDC_DC_RADIO_DST_PART        4023
#define IDC_DC_BTN_CHOOSE_DST_PART   4024
#define IDC_DC_STATIC_DST_PART       4025
#define IDC_DC_STATIC_SRC_HDR        4026
#define IDC_DC_STATIC_DST_HDR        4027

// ---- IDD_BADSECTORSCAN (CBadSectorScanDialog) ----
#define IDC_BSS_COMBO_DRIVE              5001
#define IDC_BSS_RADIO_WHOLE              5002
#define IDC_BSS_RADIO_PART               5003
#define IDC_BSS_RADIO_RANGE              5004
#define IDC_BSS_BTN_CHOOSE_PART          5005
#define IDC_BSS_STATIC_PART              5006
#define IDC_BSS_EDIT_RANGE_START         5007
#define IDC_BSS_EDIT_RANGE_COUNT         5008
#define IDC_BSS_STATIC_MAP               5009
#define IDC_BSS_STATIC_STATUS            5010
#define IDC_BSS_LIST_BAD                 5011
#define IDC_BSS_BTN_START                5012
#define IDC_BSS_BTN_STOP                 5013
#define IDC_BSS_BTN_CLOSE                5014
#define IDC_BSS_STATIC_RANGE_START_LBL   5015
#define IDC_BSS_STATIC_RANGE_COUNT_LBL   5016
#define IDC_BSS_BTN_REPAIR               5017

// ---- IDD_EXTENDPARTITION (CExtendPartitionDialog) ----
#define IDC_EX_STATIC_INFO     5101
#define IDC_EX_STATIC_CUR      5102
#define IDC_EX_STATIC_AVAIL    5103
#define IDC_EX_EDIT_MB         5104
#define IDC_EX_STATIC_MAXHINT  5105
#define IDC_EX_BTN_EXTEND      5106
#define IDC_EX_BTN_CANCEL      5107
#define IDC_EX_STATIC_STATUS   5108

// ---- IDD_SMARTHEALTH (CSmartHealthDialog) ----
#define IDC_SH_COMBO_DRIVE        6001
#define IDC_SH_BTN_REFRESH        6002
#define IDC_SH_BTN_CHECK          6003
#define IDC_SH_STATIC_OVERALL     6004
#define IDC_SH_LIST_ATTRIBUTES    6005
#define IDC_SH_STATIC_CAVEAT      6006
#define IDC_SH_BTN_CLOSE          6007

// ---- IDD_MBRGPTCONVERT (CMbrGptConvertDialog) ----
#define IDC_MG_COMBO_DRIVE       7001
#define IDC_MG_BTN_ANALYZE       7002
#define IDC_MG_LIST_PARTITIONS   7003
#define IDC_MG_STATIC_RESULT     7004
#define IDC_MG_BTN_SAVE_BACKUP   7005
#define IDC_MG_STATIC_BACKUP     7006
#define IDC_MG_BTN_CONVERT       7007
#define IDC_MG_BTN_RESTORE       7008
#define IDC_MG_BTN_CLOSE         7009

// ---- IDD_PARTITIONEDIT (CPartitionEditDialog) ----
#define IDC_PE_COMBO_DRIVE          8001
#define IDC_PE_BTN_LOAD             8002
#define IDC_PE_LIST_PARTITIONS      8003
#define IDC_PE_CHK_MBR_ACTIVE       8010
#define IDC_PE_COMBO_MBR_TYPE       8011
#define IDC_PE_CHK_MBR_HIDDEN       8012
#define IDC_PE_EDIT_GPT_NAME        8020
#define IDC_PE_COMBO_GPT_TYPE       8021
#define IDC_PE_CHK_GPT_LEGACY_BOOT  8022
#define IDC_PE_CHK_GPT_NO_LETTER    8023
#define IDC_PE_BTN_APPLY            8030
#define IDC_PE_STATIC_STATUS        8031
#define IDC_PE_STATIC_CUR_LETTER    8040
#define IDC_PE_COMBO_NEW_LETTER     8041
#define IDC_PE_BTN_ASSIGN_LETTER    8042
#define IDC_PE_BTN_REMOVE_LETTER    8043
#define IDC_PE_BTN_CLOSE            8050
#define IDC_PE_BTN_DELETE           8060
#define IDC_PE_BTN_EXTEND           8061

// ---- IDD_WIPEDISK (CWipeDiskDialog) ----
#define IDC_WD_COMBO_DRIVE       9001
#define IDC_WD_RADIO_WHOLE       9002
#define IDC_WD_RADIO_PART        9003
#define IDC_WD_BTN_CHOOSE_PART   9004
#define IDC_WD_STATIC_PART       9005
#define IDC_WD_COMBO_PATTERN     9006
#define IDC_WD_BTN_REFRESH       9007
#define IDC_WD_STATIC_SUMMARY    9008
#define IDC_WD_PROGRESS          9009
#define IDC_WD_STATIC_STATUS     9010
#define IDC_WD_BTN_START         9011
#define IDC_WD_BTN_CANCEL        9012
#define IDC_WD_BTN_CLOSE         9013

// ---- IDD_EFIBOOT (CEfiBootDialog) ----
#define IDC_EB_LIST_ENTRIES     10001
#define IDC_EB_BTN_MOVE_UP      10002
#define IDC_EB_BTN_MOVE_DOWN    10003
#define IDC_EB_BTN_APPLY_ORDER  10004
#define IDC_EB_BTN_SET_NEXT     10005
#define IDC_EB_BTN_CLEAR_NEXT   10006
#define IDC_EB_BTN_DELETE       10007
#define IDC_EB_BTN_REFRESH      10008
#define IDC_EB_STATIC_STATUS    10009
#define IDC_EB_BTN_CLOSE        10010

// ---- IDD_LOSTPARTITION (CLostPartitionDialog) ----
#define IDC_LP_COMBO_DRIVE       11001
#define IDC_LP_STATIC_FREESPACE  11002
#define IDC_LP_LIST_CANDIDATES   11003
#define IDC_LP_STATIC_STATUS     11004
#define IDC_LP_BTN_START         11005
#define IDC_LP_BTN_STOP          11006
#define IDC_LP_BTN_CLOSE         11007
#define IDC_LP_PROGRESS          11008

// ---- IDD_FILERECOVERY (CFileRecoveryDialog) ----
#define IDC_FR_COMBO_DRIVE       12001
#define IDC_FR_BTN_CHOOSE_PART   12002
#define IDC_FR_STATIC_PART       12003
#define IDC_FR_EDIT_CUSTOM_LBA   12004
#define IDC_FR_BTN_OPEN          12005
#define IDC_FR_TREE              12006
#define IDC_FR_STATIC_INFO       12007
#define IDC_FR_BTN_RECOVER       12008
#define IDC_FR_STATIC_STATUS     12009
#define IDC_FR_BTN_CLOSE         12010
