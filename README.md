# Catalyst Users for Unreal Engine

UE5.8 integration following the Catalyst plugin structure: a runtime module,
per-GameInstance UCUSubsystem and an editor test module. Foundation and Connect
are dependencies. The host configures/starts a named Connect connection; Users
shares it and owns no socket or worker thread.

```cpp
auto* Users = GameInstance->GetSubsystem<UCUSubsystem>();
Users->UseConnection(TEXT("Users"));
auto* User = NewObject<UCUUser>(GameInstance); // Host retains through UPROPERTY.
User->SetUserName(TEXT("Ada"));
Users->CreateUser(User); // Optional UCUUserOperation; framework owns admitted work.
```

Confirmed creation adopts the original UObject with stable identity/revision.
`GetUser(ExactName)` returns an optional operation and reuses the same cached
instance in that subsystem. Absence reports NotFound; ambiguous names refuse.
Observe OnChanged/OnError, sync state and pending edits; subscribe to the subsystem's
OnFabricError for scope errors. MagritteDescription uses our C++ port and native
accessors, with server-owned fields read-only, ready for future generated UI.

One bounded coordinator refreshes full state and submits edits. ConfigureLiveUsers
changes capacity/interval/automatic refresh live. Disable automatic refresh and
call RefreshLiveUsers for explicit control. Conflicts retain drafts; ResolveConflict
accepts remote state or explicitly rebases local state. Uncertain writes never
replay automatically. The caller owns reconnect; the Fabric renegotiates and recovers
creation receipts on the same connection scope. UseConnection refuses retargeting
while Users/operations are retained. Current update permission is creator-peer scoped;
account/tenant authorization is future work.

Enable server operations 3/4/5/6/65534 and an authoritative Users binding.
Legacy 1/2 use Prepare, CreateUserSnapshot and GetUserSnapshot with FCUUserSnapshot;
FCUUser remains a C++ alias. Legacy Blueprint nodes use Snapshot names. Only
Completed carries a valid snapshot, and reconnect requires fresh legacy Prepare.

Install as Plugins/CatalystUsers beside CatalystFoundation and CatalystConnect,
after staging Connect's pinned Quinn/Magritte archives. See the service repository's
`docs/Connect-CreateUser.md` for protocol, recovery limits and security scope.
`Catalyst.Users.Native.CreateUser` and `scripts/qualify-platform-users.py` exercise
the actual subsystems against a configured GemStone authority. No generated UI,
push subscription, graph/4 transfer or non-Mac engine qualification is claimed.

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
