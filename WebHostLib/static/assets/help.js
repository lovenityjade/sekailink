const supportForm = document.getElementById("support-ticket-form");
if (supportForm) {
  const status = document.getElementById("support-ticket-status");
  supportForm.addEventListener("submit", async (event) => {
    event.preventDefault();
    const formData = new FormData(supportForm);
    const payload = {
      category: formData.get("category"),
      subject: (formData.get("subject") || "").trim(),
      message: (formData.get("message") || "").trim(),
    };
    if (status) status.textContent = "Sending ticket...";
    const res = await fetch("/api/support/tickets", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    if (!res.ok) {
      const data = await res.json().catch(() => ({}));
      if (status) status.textContent = data.error || "Unable to send ticket.";
      return;
    }
    if (status) status.textContent = "Ticket sent. We will reply in private messages.";
    supportForm.reset();
  });
}
