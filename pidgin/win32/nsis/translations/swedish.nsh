;;
;;  swedish.nsh
;;
;;  Swedish language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Tore Lundqvist <tlt@mima.x.se>, 2003.
;;  Author: Peter Hjalmarsson <xake@telia.com>, 2005.
;;  Version 3

; Startup Checks
!define INSTALLER_IS_RUNNING			"Installationsprogrammet k�rs redan."
!define PIDGIN_IS_RUNNING			"En instans av Pidgin k�rs redan. Avsluta Pidgin och f�rs�k igen."

; License Page
!define PIDGIN_LICENSE_BUTTON			"N�sta >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) �r utgivet under GPL. Licensen finns tillg�nglig h�r f�r informationssyften enbart. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Snabbmeddelandeklient (obligatorisk)"
!define GTK_SECTION_TITLE			"GTK+-k�rmilj� (obligatorisk)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE 		"Genv�gar"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE 	"Skrivbord"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "Startmeny"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgins k�rnfiler och DLL:er"
!define GTK_SECTION_DESCRIPTION			"En GUI-verktygsupps�ttning f�r flera olika plattformar som Pidgin anv�nder."

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   	"Genv�gar f�r att starta Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   		"Skapar en genv�g till Pidgin p� skrivbordet"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   	"Skapar ett till�gg i startmenyn f�r Pidgin"

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Bes�k Windows-Pidgin hemsida"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Kunde inte avinstallera den nuvarande versionen av Pidgin. Den nya versionen kommer att installeras utan att ta bort den f�r n�rvarande installerade versionen."

; GTK+ Section Prompts

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI Hanterare"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         	"Avinstalleraren kunde inte hitta registerv�rden f�r Pidgin.$\rAntagligen har en annan anv�ndare installerat applikationen."
!define un.PIDGIN_UNINSTALL_ERROR_2         	"Du har inte r�ttigheter att avinstallera den h�r applikationen."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"St�d f�r r�ttstavning"
!define PIDGIN_SPELLCHECK_ERROR			"Fel vid installation f�r r�ttstavning"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"St�d f�r R�ttstavning.  (Internetanslutning kr�vs f�r installation)"
