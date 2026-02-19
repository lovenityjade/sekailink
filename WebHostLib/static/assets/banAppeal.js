const form = document.getElementById("ban-appeal-form");

if (form) {
  const textarea = form.querySelector("textarea");
  const button = form.querySelector("button");
  const status = document.getElementById("ban-appeal-status");

  const setStatus = (message, isError = false) => {
    if (!status) return;
    status.textContent = message;
    status.dataset.state = isError ? "error" : "success";
  };

  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const message = (textarea?.value || "").trim();

    if (message.length < 20) {
      setStatus("Please provide at least 20 characters.", true);
      return;
    }

    button.disabled = true;
    textarea.disabled = true;
    setStatus("Submitting your appeal...");

    try {
      const response = await fetch("/api/support/ban-appeal", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ message }),
      });

      if (!response.ok) {
        const payload = await response.json().catch(() => ({}));
        throw new Error(payload.error || "Unable to submit appeal.");
      }

      setStatus("Appeal submitted. Our team will review it shortly.");
      textarea.value = "";
    } catch (error) {
      setStatus(error.message || "Unable to submit appeal.", true);
      textarea.disabled = false;
      button.disabled = false;
    }
  });
}
