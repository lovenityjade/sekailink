!macro customInstall
  CreateDirectory "$SMPROGRAMS\SekaiLink"
  CreateShortCut "$SMPROGRAMS\SekaiLink\SekaiLink.lnk" "$INSTDIR\SekaiLink-bootstrapper.cmd" "" "$INSTDIR\SekaiLink Client.exe" 0
  CreateShortCut "$DESKTOP\SekaiLink.lnk" "$INSTDIR\SekaiLink-bootstrapper.cmd" "" "$INSTDIR\SekaiLink Client.exe" 0
!macroend

!macro customUnInstall
  Delete "$DESKTOP\SekaiLink.lnk"
  Delete "$SMPROGRAMS\SekaiLink\SekaiLink.lnk"
  RMDir "$SMPROGRAMS\SekaiLink"
!macroend
