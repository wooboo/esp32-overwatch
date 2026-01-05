import { route } from 'preact-router';

export function Navigation() {
  return (
    <nav class="max-w-[1100px] mx-auto mb-4">
      <div class="bg-card border border-border rounded-xl p-2 flex gap-2">
        <NavLink href="/" label="Status" />
        <NavLink href="/settings" label="Settings" />
      </div>
    </nav>
  );
}

function NavLink({ href, label }: { href: string; label: string }) {
  const isActive = window.location.pathname === href;
  const base = 'px-4 py-2 rounded-lg font-bold transition-colors';
  const classes = isActive
    ? `${base} bg-accent text-bg`
    : `${base} bg-chip-bg text-text hover:bg-chip-ok`;

  return (
    <a
      href={href}
      class={classes}
      onClick={(e) => {
        e.preventDefault();
        route(href);
      }}
    >
      {label}
    </a>
  );
}
