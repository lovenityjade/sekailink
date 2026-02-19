const overlay = document.getElementById("skl-terms-overlay");
if (overlay) {
  const checkbox = document.getElementById("skl-terms-checkbox");
  const button = document.getElementById("skl-terms-accept");
  const error = document.getElementById("skl-terms-error");

  const syncState = () => {
    button.disabled = !checkbox.checked;
  };

  checkbox.addEventListener("change", syncState);
  syncState();

  button.addEventListener("click", async () => {
    button.disabled = true;
    error.textContent = "";
    try {
      const res = await fetch("/api/auth/terms", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ accept: true }),
      });
      if (!res.ok) {
        throw new Error("Terms acceptance failed.");
      }
      overlay.remove();
    } catch (err) {
      error.textContent = "Unable to save your acceptance. Please try again.";
      button.disabled = !checkbox.checked;
    }
  });
}
