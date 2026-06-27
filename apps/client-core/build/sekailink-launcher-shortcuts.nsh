!macro customInstall
  CreateDirectory "$SMPROGRAMS\SekaiLink"
  Delete "$SMPROGRAMS\SekaiLink\SekaiLink Client.lnk"
  Delete "$SMPROGRAMS\SekaiLink\SekaiLink Bootstrapper.lnk"
  Delete "$DESKTOP\SekaiLink Client.lnk"
  Delete "$DESKTOP\SekaiLink Bootstrapper.lnk"
  CreateShortCut "$SMPROGRAMS\SekaiLink\SekaiLink.lnk" "$INSTDIR\SekaiLink Bootloader\SekaiLink Bootloader.exe" "" "$INSTDIR\SekaiLink Bootloader\SekaiLink Bootloader.exe" 0
  CreateShortCut "$SMPROGRAMS\SekaiLink\Uninstall SekaiLink.lnk" "$INSTDIR\Uninstall SekaiLink Client.exe" "" "$INSTDIR\Uninstall SekaiLink Client.exe" 0
  CreateShortCut "$DESKTOP\SekaiLink.lnk" "$INSTDIR\SekaiLink Bootloader\SekaiLink Bootloader.exe" "" "$INSTDIR\SekaiLink Bootloader\SekaiLink Bootloader.exe" 0
!macroend

!macro customUnInstall
  Delete "$DESKTOP\SekaiLink.lnk"
  Delete "$DESKTOP\SekaiLink Client.lnk"
  Delete "$DESKTOP\SekaiLink Bootstrapper.lnk"
  Delete "$SMPROGRAMS\SekaiLink\SekaiLink.lnk"
  Delete "$SMPROGRAMS\SekaiLink\SekaiLink Client.lnk"
  Delete "$SMPROGRAMS\SekaiLink\SekaiLink Bootstrapper.lnk"
  Delete "$SMPROGRAMS\SekaiLink\Uninstall SekaiLink.lnk"
  RMDir "$SMPROGRAMS\SekaiLink"
!macroend
