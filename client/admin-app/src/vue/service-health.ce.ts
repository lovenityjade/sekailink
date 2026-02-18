import { defineCustomElement, h } from "vue";

type ServiceEntry = {
  key?: string;
  active?: string;
  enabled?: string;
  unit?: string;
  ok?: boolean;
  pid?: number;
  uptime_seconds?: number;
};

const formatSeconds = (value: number) => {
  if (!Number.isFinite(value) || value <= 0) return "0s";
  const total = Math.floor(value);
  const h = Math.floor(total / 3600);
  const m = Math.floor((total % 3600) / 60);
  const s = total % 60;
  if (h > 0) return `${h}h ${m}m ${s}s`;
  if (m > 0) return `${m}m ${s}s`;
  return `${s}s`;
};

const ServiceHealthWidget = defineCustomElement({
  props: {
    snapshot: { type: String, default: "{}" }
  },
  render() {
    let parsed: Record<string, ServiceEntry> = {};
    try {
      parsed = JSON.parse(String(this.snapshot || "{}"));
    } catch {
      parsed = {};
    }
    const entries = Object.entries(parsed);

    return h("div", { class: "vue-health-grid" }, entries.length
      ? entries.map(([key, service]) => {
          const ok = Boolean(service?.ok);
          return h("article", { class: `vue-health-card${ok ? " ok" : " err"}` }, [
            h("header", { class: "vue-health-head" }, [
              h("strong", key),
              h("span", { class: "vue-pill" }, ok ? "healthy" : "issue")
            ]),
            h("div", { class: "vue-health-body" }, [
              h("p", [h("span", "active"), h("code", String(service?.active || "unknown"))]),
              h("p", [h("span", "enabled"), h("code", String(service?.enabled || "unknown"))]),
              h("p", [h("span", "pid"), h("code", String(service?.pid || 0))]),
              h("p", [h("span", "uptime"), h("code", formatSeconds(Number(service?.uptime_seconds || 0)))])
            ])
          ]);
        })
      : [h("p", { class: "vue-empty" }, "No service telemetry available yet.")]
    );
  }
});

if (!customElements.get("service-health-widget")) {
  customElements.define("service-health-widget", ServiceHealthWidget);
}
