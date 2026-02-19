async function copyRoomToClipboard(host, port) {
  await navigator.clipboard.writeText(host + ":" + port)
  document.getElementById("snackbar").classList.add("show");
  setTimeout(() => {
    document.getElementById("snackbar").classList.remove("show");
  }, 3000);
}