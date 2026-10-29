# Catalyst Users for Unreal Engine

UE5.8 Users integration following the Catalyst Connect plugin template. The runtime
module provides Blueprint-compatible `FCUUser` and per-GameInstance `UCUSubsystem`;
a separate editor test module owns native qualification. There is no empty editor
module or actor-owned socket. Foundation and Connect are plugin dependencies.

`FCUUser::MagritteDescription()` owns its native fields, validation/presentation
metadata and accessors. The generic `TCCObjectEndpoint<FCUUser, FCUUser>` derives
codecs and agreement from those descriptions. No Users network field list or
codec is maintained. Peer bytes cannot select classes, accessors, routes or code.

Stage Connect's pinned Quinn and Magritte archives first, then install this repo
as `Plugins/CatalystUsers` alongside `CatalystFoundation` and `CatalystConnect`.
Enable `CatalystUsers`. Its module declares a GameInstance initialization dependency
on the shared Connect subsystem.

The host configures/starts a named Connect connection. Obtain Users with
`GameInstance->GetSubsystem<UCUSubsystem>()`, call `UseConnection(Name)`, and call
`Prepare` after transport readiness. Then `CreateUser(User, Callback)` returns
`Completed` with a native value only after the authoritative server's committed
response. In Blueprint, consume the User argument only when Outcome is Completed;
other outcomes carry the default empty struct. No automatic mutation replay occurs.

Call `GetUser(ExactName, Callback)` for exact, case-sensitive lookup. `Completed`
carries the native user, `NotFound` means explicit absence, and `Refused` includes
ambiguous names. No default user is substituted. Enable server operations 1, 2 and
65534; `Prepare` establishes both agreements. The name-only lookup request reuses
`FCUUser` name metadata and the result uses the unchanged user description.

The generic endpoint fences agreement to the connection generation and retains
its immutable result schema until terminal observation. Explicit reconnect needs
a fresh probe. Runtime-created default GemStone workers retain all service routing,
capability checks and commit/rollback ownership. Account/tenant authorization and
stable user identifiers remain server-domain work.

The `Catalyst.Users.Native.CreateUser` automation case uses the actual GameInstance
subsystems and delegates against a configured authoritative Users service. It
checks description agreement, a held pre-commit insertion, native result, rollback,
super-user rejection, stale agreement and reconnect. The outside controller verifies
visibility in independent Gem sessions and removes only its owned Runtime/users.
See the service repository's `scripts/qualify-platform-users.py` and the checked-in
platform qualification evidence. Mac arm64 UE5.8 is qualified; other engine/platform
targets and interactive gameplay UI remain separate checks.

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
