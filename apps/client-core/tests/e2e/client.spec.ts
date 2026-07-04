import { test, expect } from "@playwright/test";

const mockApi = async (page: any) => {
  await page.route("**/api/lobbies", async (route: any) => {
    if (route.request().method() === "GET") {
      return route.fulfill({
        status: 200,
        contentType: "application/json",
        body: JSON.stringify({ lobbies: [{ id: "abc", name: "Test Lobby", owner: "Host" }] })
      });
    }
    return route.fulfill({
      status: 201,
      contentType: "application/json",
      body: JSON.stringify({ url: "/lobby/abc" })
    });
  });

  await page.route("**/api/yamls", async (route: any) => {
    return route.fulfill({
      status: 200,
      contentType: "application/json",
      body: JSON.stringify({ yamls: [{ id: "y1", title: "Seed", game: "Game", player_name: "P" }] })
    });
  });

  await page.route("**/api/yamls/*/raw", async (route: any) => {
    return route.fulfill({
      status: 200,
      contentType: "application/json",
      body: JSON.stringify({ yaml: "game: Game\nname: P\n" })
    });
  });

  await page.route("**/api/me", async (route: any) => {
    return route.fulfill({
      status: 200,
      contentType: "application/json",
      body: JSON.stringify({ discord_id: "1", username: "User", terms_version: "v1" })
    });
  });

  await page.route("**/api/social/settings", async (route: any) => {
    return route.fulfill({
      status: 200,
      contentType: "application/json",
      body: JSON.stringify({ presence_status: "online", dm_policy: "anyone" })
    });
  });

  await page.route("**/api/auth/terms", async (route: any) => {
    return route.fulfill({ status: 200, contentType: "application/json", body: "{}" });
  });
};

test("Room list create + Game Manager edit", async ({ page }) => {
  await mockApi(page);

  await page.goto("/#/rooms");
  await expect(page.getByText("Room List")).toBeVisible();
  await expect(page.getByText("Test Lobby")).toBeVisible();

  await page.getByRole("button", { name: "Create Room" }).first().click();
  await page.getByPlaceholder("Room name").fill("My Room");
  await page.getByPlaceholder("Short description").fill("Hello");
  await page.getByRole("button", { name: "Create Room" }).nth(1).click();

  await page.goto("/#/dashboard/yaml/new");
  await expect(page.getByText("Game Manager")).toBeVisible();
  await page.getByRole("button", { name: "Edit" }).click();
  await expect(page.getByText("YAML Editor (Advanced)")).toBeVisible();
});

test("Account download flow", async ({ page }) => {
  await mockApi(page);

  await page.goto("/#/account");
  await expect(page.getByText("SekaiLink Control Panel")).toBeVisible();
  await page.getByRole("button", { name: "Download" }).click();
});
