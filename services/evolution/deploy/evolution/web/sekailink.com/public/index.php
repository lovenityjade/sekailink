<?php

declare(strict_types=1);

$config = require dirname(__DIR__) . '/app/config.php';

$escape = static fn(string $value): string => htmlspecialchars($value, ENT_QUOTES, 'UTF-8');
$siteName = $escape($config['site_name']);
$tagline = $escape($config['tagline']);
$heroTitle = $escape($config['hero_title']);
$heroSubtitle = $escape($config['hero_subtitle']);
$heroMicro = $escape($config['hero_micro']);
$discordUrl = $escape($config['discord_url']);
$downloadUrl = $escape($config['download_url']);
$roomBrowserUrl = $escape($config['room_browser_url']);
$loginUrl = $escape($config['login_url']);
$registerUrl = $escape($config['register_url']);
$supportEmail = $escape($config['support_email']);
$statusLabel = $escape($config['status_label']);
$downloadLabel = $escape($config['download_label']);
?>
<!DOCTYPE html>
<html lang="en" data-theme="circuit-forge">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title><?= $siteName ?> — <?= $tagline ?></title>
  <meta name="description" content="<?= $heroSubtitle ?>">
  <meta name="theme-color" content="#050508">
  <link rel="icon" href="/assets/favicon.ico">
  <meta property="og:title" content="<?= $siteName ?> — <?= $tagline ?>">
  <meta property="og:description" content="<?= $heroSubtitle ?>">
  <meta property="og:type" content="website">
  <meta property="og:image" content="/assets/branding/landing-announcement.png">
  <link rel="stylesheet" href="/css/style.css">
</head>
<body>
<div class="skl-bg-gradient"></div>
<canvas id="skl-bg-canvas"></canvas>
<div class="skl-bg-overlay"></div>
<div class="skl-wave"></div>
<div class="skl-wave"></div>
<div class="skl-wave"></div>

<main class="skl-maintenance" aria-labelledby="maintenance-title">
  <div class="skl-maintenance-card">
    <div class="skl-maintenance-spinner" aria-hidden="true"></div>
    <h1 id="maintenance-title">Maintenance in progress</h1>
  </div>
</main>
</body>
</html>
<?php return; ?>

<header class="skl-topbar" id="skl-topbar">
  <div class="skl-topbar-inner">
    <a href="#hero" class="skl-topbar-brand">
      <img class="skl-topbar-brand-icon" src="/assets/branding/sekailink-logo-image.png" alt="<?= $siteName ?>" width="34" height="34">
      <img class="skl-topbar-brand-text" src="/assets/branding/sekailink-logo-text.png" alt="<?= $siteName ?>">
    </a>
    <nav class="skl-topbar-links">
      <a href="#features">Features</a>
      <a href="#runtime">Runtime</a>
      <a href="#games">Games</a>
      <a href="#download">Download</a>
      <a href="#support">Support</a>
    </nav>
    <a class="skl-btn ghost" href="<?= $loginUrl ?>">Login</a>
    <a class="skl-btn primary skl-shine" href="<?= $registerUrl ?>">Create Account</a>
  </div>
</header>

<div class="skl-landing">
  <section class="skl-announcement" id="stress-test" aria-labelledby="stress-test-title">
    <h1 class="skl-sr-only" id="stress-test-title">SekaiLink Public Stress Test is now available</h1>
    <div class="skl-announcement-frame skl-reveal">
      <img
        class="skl-announcement-image"
        src="/assets/branding/landing-announcement.png"
        width="1672"
        height="941"
        alt="SekaiLink Public Stress Test is now available. Join our Discord, register, or go to the downloads."
        fetchpriority="high"
      >
      <a class="skl-announcement-hotspot skl-announcement-hotspot--discord" href="<?= $discordUrl ?>" target="_blank" rel="noopener" aria-label="Join the SekaiLink Discord"></a>
      <a class="skl-announcement-hotspot skl-announcement-hotspot--register" href="<?= $registerUrl ?>" aria-label="Register for SekaiLink"></a>
      <a class="skl-announcement-hotspot skl-announcement-hotspot--download" href="<?= $downloadUrl ?>" aria-label="Go to the SekaiLink downloads"></a>
    </div>
  </section>

  <section class="skl-hero" id="hero">
    <div class="skl-hero-inner">
      <div class="skl-hero-copy skl-reveal">
        <p class="skl-eyebrow"><?= $statusLabel ?></p>
        <h1 class="skl-hero-title"><?= $heroTitle ?></h1>
        <p class="skl-hero-subtitle"><?= $heroSubtitle ?></p>
        <p class="skl-hero-micro"><?= $heroMicro ?></p>
        <div class="skl-hero-cta">
          <a class="skl-btn primary skl-btn-lg skl-shine" href="<?= $downloadUrl ?>">Download</a>
          <a class="skl-btn ghost skl-btn-lg" href="<?= $roomBrowserUrl ?>">Browse Rooms</a>
          <a class="skl-btn ghost skl-btn-lg" href="<?= $discordUrl ?>" target="_blank" rel="noopener">Discord</a>
        </div>
        <div class="skl-hero-badges">
          <span class="skl-badge"><span class="skl-badge-dot"></span> <?= $downloadLabel ?></span>
          <span class="skl-badge"><span class="skl-badge-dot"></span> Native server migration in progress</span>
          <span class="skl-badge"><span class="skl-badge-dot"></span> Live + async room direction</span>
        </div>
      </div>

      <div class="skl-hero-visual skl-reveal-right">
        <div class="skl-mockup-stack">
          <div class="skl-mockup skl-mockup-main">
            <div class="skl-mockup-titlebar">
              <span class="skl-mockup-dot"></span>
              <span class="skl-mockup-dot"></span>
              <span class="skl-mockup-dot"></span>
              <span class="skl-mockup-app-name">SEKAILINK</span>
            </div>
            <div class="skl-mockup-content">
              <div class="skl-mockup-sidebar">
                <div class="skl-mockup-nav-item active"></div>
                <div class="skl-mockup-nav-item"></div>
                <div class="skl-mockup-nav-item"></div>
              </div>
              <div class="skl-mockup-body">
                <div class="skl-mockup-header-bar"></div>
                <div class="skl-mockup-row"></div>
                <div class="skl-mockup-row short"></div>
                <div class="skl-mockup-label">READY TO LAUNCH</div>
                <div class="skl-mockup-launch-btn"></div>
              </div>
            </div>
          </div>
          <div class="skl-mockup skl-mockup-secondary">
            <div class="skl-mockup-tracker-badge">ROOM</div>
            <div class="skl-mockup-grid">
              <div class="skl-mockup-item"></div>
              <div class="skl-mockup-item lit"></div>
              <div class="skl-mockup-item"></div>
              <div class="skl-mockup-item lit"></div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </section>

  <section class="skl-features" id="features">
    <p class="skl-eyebrow skl-reveal">Platform Direction</p>
    <h2 class="skl-section-title skl-reveal">A cleaner stack, with less glue and more control.</h2>
    <div class="skl-feature-grid">
      <?php foreach ($config['feature_cards'] as $card): ?>
        <article class="skl-feature-card glass glow-border skl-reveal">
          <div class="skl-feature-icon"><?= $escape($card['icon']) ?></div>
          <h3><?= $escape($card['title']) ?></h3>
          <p><?= $escape($card['body']) ?></p>
        </article>
      <?php endforeach; ?>
    </div>
  </section>

  <section class="skl-steps" id="runtime">
    <p class="skl-eyebrow skl-reveal">Current Runtime Shape</p>
    <h2 class="skl-section-title skl-reveal">Native services already moving into place.</h2>
    <div class="skl-steps-grid">
      <div class="skl-step glass glow-border skl-reveal">
        <div class="skl-step-number">01</div>
        <h3>Link</h3>
        <p>Native room server, audit, projection writes, and admin agent foundations now exist as real long-lived services.</p>
      </div>
      <div class="skl-step glass glow-border skl-reveal">
        <div class="skl-step-number">02</div>
        <h3>Worlds</h3>
        <p>Generation orchestration moved into native C++, while the existing generator itself remains external for safety.</p>
      </div>
      <div class="skl-step glass glow-border skl-reveal">
        <div class="skl-step-number">03</div>
        <h3>Evolution</h3>
        <p>Apache2 + PHP hosts the public site while native projection-query layers prepare the next API surfaces.</p>
      </div>
      <div class="skl-step glass glow-border skl-reveal">
        <div class="skl-step-number">04</div>
        <h3>Client Direction</h3>
        <p>The client will be refit to these services and structured room contracts instead of the old mixed toolchain path.</p>
      </div>
    </div>
  </section>

  <section class="skl-games" id="games">
    <p class="skl-eyebrow skl-reveal">Integrated Games</p>
    <h2 class="skl-section-title skl-reveal">Verified favorites and active test targets.</h2>
    <div class="skl-games-count skl-reveal">30+</div>
    <p class="skl-section-subtitle">Representative titles already used in current runtime and tracker work.</p>
    <div class="skl-feature-grid">
      <?php foreach ($config['games'] as $game): ?>
        <article class="skl-feature-card glass glow-border skl-reveal">
          <div class="skl-feature-icon">•</div>
          <h3><?= $escape($game) ?></h3>
          <p>Live-room and tracker workflow target inside the SekaiLink migration path.</p>
        </article>
      <?php endforeach; ?>
    </div>
  </section>

  <section class="skl-verified" id="download">
    <div class="skl-verified-inner">
      <div class="skl-verified-copy skl-reveal-left">
        <p class="skl-eyebrow">Download</p>
        <h2 class="skl-section-title">Public test build and account entry points.</h2>
        <p>Use the current download to join testing, then move into room browsing, account setup, and support through the unified `sekailink.com` surface.</p>
        <ul class="skl-check-list">
          <li><a href="<?= $downloadUrl ?>">Download current build</a></li>
          <li><a href="<?= $registerUrl ?>">Create account</a></li>
          <li><a href="<?= $loginUrl ?>">Login</a></li>
          <li><a href="<?= $roomBrowserUrl ?>">Browse rooms</a></li>
        </ul>
      </div>
      <div class="skl-verified-visual skl-reveal-right">
        <div class="skl-room-mockup glass">
          <div class="skl-room-header">
            <span class="skl-room-title">Current focus</span>
            <span class="skl-room-code">PHP + Apache2</span>
          </div>
          <div class="skl-room-players">
            <div class="skl-room-player ready"><span class="skl-player-status"></span><span class="skl-player-name">Unified public domain</span></div>
            <div class="skl-room-player ready"><span class="skl-player-status"></span><span class="skl-player-name">Uploadable website package</span></div>
            <div class="skl-room-player ready"><span class="skl-player-status"></span><span class="skl-player-name">Room entry routing</span></div>
            <div class="skl-room-player ready"><span class="skl-player-status"></span><span class="skl-player-name">Future auth/account wiring</span></div>
          </div>
        </div>
      </div>
    </div>
  </section>

  <section class="skl-streamers" id="support">
    <div class="skl-streamer-inner">
      <div class="skl-streamer-visual skl-reveal-left">
        <div class="skl-stream-mockup">
          <div class="skl-stream-window game"><div class="skl-stream-label">Site</div></div>
          <div class="skl-stream-window tracker"><div class="skl-stream-label">Runtime</div></div>
          <div class="skl-stream-window chat"><div class="skl-stream-label">Support</div></div>
        </div>
      </div>
      <div class="skl-streamer-copy skl-reveal-right">
        <p class="skl-eyebrow">Support</p>
        <h2 class="skl-section-title">Public web now, deeper services next.</h2>
        <p>This package is intentionally simple to upload now. It gives you the public entry point while the rest of the server refactor continues behind it.</p>
        <ul class="skl-check-list">
          <li>Discord: <a href="<?= $discordUrl ?>" target="_blank" rel="noopener"><?= $discordUrl ?></a></li>
          <li>Email: <a href="mailto:<?= $supportEmail ?>"><?= $supportEmail ?></a></li>
          <li>Public domain target: <strong>sekailink.com</strong></li>
          <li>Hosting target: <strong>Apache2 + PHP on evolution</strong></li>
        </ul>
      </div>
    </div>
  </section>
</div>

<script src="/js/main.js"></script>
</body>
</html>
