// resource.h - Resource identifiers
#pragma once

// Dialog template IDs
#define IDR_MAINFRAME       128
#define IDD_MAIN            100
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
#define IDD_FILEUNLOCKER       112
#define IDD_EXTENDPARTITION    113
#define IDD_STARTUPMANAGER     114
#define IDD_ADDSTARTUPENTRY    115
#define IDD_REGISTRYSCANNER    116

// ---- IDD_MAIN (CDiskForgeDlg / CFormView) control IDs ----
#define IDC_COMBO_DRIVES         1001
#define IDC_BTN_OPEN             1002
#define IDC_STATIC_DRIVEINFO     1003
#define IDC_EDIT_SECTOR          1004
#define IDC_BTN_GOTO             1005
#define IDC_BTN_PREV             1006
#define IDC_BTN_NEXT             1007
#define IDC_BTN_FIRST            1008
#define IDC_BTN_LAST             1009
#define IDC_HEXVIEW              1010
#define IDC_STATIC_SECTOR        1011
#define IDC_BTN_REFRESH          1012
#define IDC_PROGRESS             1013
#define IDC_STATIC_STATUS        1014
#define IDC_BTN_PARTITIONS       1015
#define IDC_BTN_DISKCOPY         1016
#define IDC_BTN_BADSECTORSCAN    1017
#define IDC_BTN_SMARTHEALTH      1018
#define IDC_BTN_MBRGPT           1019
#define IDC_BTN_PARTEDIT         1020
#define IDC_BTN_WIPEDISK         1021
#define IDC_BTN_EFIBOOT          1022
#define IDC_BTN_LOSTPART         1023
#define IDC_BTN_FILERECOVERY     1024
#define IDC_STATIC_SIDEBAR_TITLE 1025
#define IDC_STATIC_SIDEBAR_BG    1026
#define IDC_BTN_FILEUNLOCKER     1027
#define IDC_BTN_STARTUP          1028
#define IDC_BTN_REGSCAN          1029

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

// ---- IDD_FILEUNLOCKER (CFileUnlockerDialog) ----
#define IDC_FU_EDIT_FILE_PATH    13001
#define IDC_FU_BTN_BROWSE        13002
#define IDC_FU_LIST_LOCKERS      13003
#define IDC_FU_BTN_REFRESH       13004
#define IDC_FU_BTN_KILL_SELECTED 13005
#define IDC_FU_BTN_KILL_ALL      13006
#define IDC_FU_STATIC_STATUS     13007
#define IDC_FU_BTN_CLOSE         13008

// ---- IDD_STARTUPMANAGER (CStartupManagerDialog) ----
#define IDC_SM_LIST           14001
#define IDC_SM_BTN_ADD        14002
#define IDC_SM_BTN_DELETE     14003
#define IDC_SM_BTN_TOGGLE     14004
#define IDC_SM_BTN_REFRESH    14005
#define IDC_SM_BTN_CLOSE      14006
#define IDC_SM_STATIC_STATUS  14007

// ---- IDD_ADDSTARTUPENTRY (CAddStartupEntryDialog) ----
#define IDC_AS_STATIC_NAME    15001
#define IDC_AS_EDIT_NAME      15002
#define IDC_AS_STATIC_CMD     15003
#define IDC_AS_EDIT_CMD       15004
#define IDC_AS_BTN_BROWSE     15005
#define IDC_AS_RADIO_HKCU     15006
#define IDC_AS_RADIO_HKLM     15007
#define IDC_AS_CHK_RUNONCE    15008
#define IDC_AS_BTN_ADD        15009
#define IDC_AS_BTN_CANCEL     15010

// ---- IDD_REGISTRYSCANNER (CRegistryScannerDialog) ----
#define IDC_RS_CHK_STARTUP      16001
#define IDC_RS_CHK_UNINSTALL    16002
#define IDC_RS_CHK_APPPATH      16003
#define IDC_RS_CHK_SHAREDDLL    16004
#define IDC_RS_BTN_SCAN         16005
#define IDC_RS_PROGRESS         16006
#define IDC_RS_LIST             16007
#define IDC_RS_BTN_FIX_SEL      16008
#define IDC_RS_BTN_FIX_ALL      16009
#define IDC_RS_BTN_BACKUP_FIX   16010
#define IDC_RS_BTN_IGNORE       16011
#define IDC_RS_BTN_CLOSE        16012
#define IDC_RS_STATIC_STATUS    16013
#define IDC_RS_STATIC_SCOPE     16014
#define IDC_RS_STATIC_PROGRESS  16015
#define IDC_RS_CHK_BROWSER_HELPER 16016
#define IDC_RS_CHK_FILE_EXT       16017
#define IDC_RS_CHK_FIREWALL       16018
#define IDC_RS_CHK_FONTS          16019
#define IDC_RS_CHK_HELP_FILES     16020
#define IDC_RS_CHK_INSTALLERS     16021
#define IDC_RS_CHK_INTERFACE      16022
#define IDC_RS_CHK_MUI_CACHE      16023
#define IDC_RS_CHK_OPEN_WITH      16024
#define IDC_RS_CHK_SOUND_EVENTS   16025
#define IDC_RS_CHK_SERVICES       16026
