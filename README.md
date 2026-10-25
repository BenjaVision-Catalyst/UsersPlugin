# Catalyst Users for Unreal Engine

Catalyst Users for Unreal Engine is the planned client integration for games
that need Catalyst accounts and user-related operations without implementing
their own Catalyst message routing. This repository is currently a placeholder:
there is no Unreal module, Blueprint API, or working account client to install
yet.

## Planned role

- Unreal-facing Users values and operations.
- Communication through Catalyst Connect, with identity and account decisions
  made by the authoritative Users service.
- Client state and presentation integration for Unreal applications.

Implementation, wire contracts, engine compatibility, and packaging remain
future work. The server component is also still being developed.

## Installation and development

Install the Catalyst development environment through
[BVC](https://github.com/BenjaVisionPro/bvc#install). There is no functional
package from this repository to install yet.

Read the [component design](https://github.com/BenjaVisionPro/catalyst-workspace/blob/main/Users/Catalyst-Users-Design.md)
for the intended scope and remaining integration work. Design documents describe
the direction; they do not establish a released implementation.

## License

This repository is licensed under the [Catalyst Developer License](LICENSE).

Each developer using Catalyst for development needs an active
BenjaVision-Catalyst subscription: **$5 per month, billed annually at $60 per
developer**, including access to the BenjaVision-Catalyst repository. New products,
updates, bug fixes, and recompilation require active developer subscriptions.
Automated build machines do not need separate subscriptions.

Products lawfully built during active subscriptions may continue to be used,
hosted, sold, and distributed indefinitely without runtime royalties. Customers
and operators do not need subscriptions solely to use or distribute those builds.
Commercial and proprietary products are permitted; this license does not require
you to publish your own source code.

Existing license grants and separately licensed third-party material remain
unaffected. See [LICENSE](LICENSE) for the complete terms.
