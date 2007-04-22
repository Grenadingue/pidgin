;;
;;  trad-chinese.nsh
;;
;;  Traditional Chineese language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page:950
;;
;;  Author: Paladin R. Liu <paladin@ms1.hinet.net>
;;  Minor updates: Ambrose C. Li <acli@ada.dhs.org>
;;
;;  Last Updated: July 5, 2005
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"安裝程式正在執行中。"
!define PIDGIN_IS_RUNNING				"Pidgin 正在執行中，請先結束這個程式後再行安裝。"
!define GTK_INSTALLER_NEEDED			"找不到符合的 GTK+ 執行環境或是需要被更新。$\r請安裝 v${GTK_MIN_VERSION} 以上版本的 GTK+ 執行環境。"

; License Page
!define PIDGIN_LICENSE_BUTTON			"下一步 >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) 採用 GNU General Public License (GPL) 授權發佈。在此列出授權書，僅作為參考之用。$_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin 主程式 (必需)"
!define GTK_SECTION_TITLE			"GTK+ 執行環境 (必需)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ 佈景主題"
!define GTK_NOTHEME_SECTION_TITLE		"不安裝佈景主題"
!define GTK_WIMP_SECTION_TITLE			"Wimp 佈景主題"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve 佈景主題"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue 佈景主題"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"捷徑"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"桌面捷徑"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "開始功能表"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgin 核心檔案及動態函式庫"
!define GTK_SECTION_DESCRIPTION			"Pidgin 所使用的跨平台圖形介面函式庫"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"建立 Pidgin 捷徑"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"在桌面建立捷徑"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"在開始功能表建立捷徑"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"發現一個舊版的 GTK+ 執行環境。您要將它升級嗎？$\r請注意：如果您不升級， $(^Name) 可能無法正確的被執行。"

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"拜訪 Windows Pidgin 網頁"

; Pidgin Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"$(^Name) (只供移除)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"安裝 GTK+ 執行環境時發生錯誤。"
!define GTK_BAD_INSTALL_PATH			"您所輸入的安裝目錄無法存取或建立。"

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"您目前的權限無法安裝 GTK+ 佈景主題。"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"移除程式無法找到 Pidgin 的安裝資訊。$\r這應該是有其他的使用者重新安裝了這個程式。"
!define un.PIDGIN_UNINSTALL_ERROR_2		"您目前的權限無法移除 Pidgin 。"
