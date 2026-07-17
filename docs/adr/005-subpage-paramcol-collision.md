# ADR-005: Subpage paramCol collision dispatch

**Status**: Accepted, long-standing project invariant (see `SPEC.md` §6).

## Context

The Oberheim protocol reuses the same `paramCol` byte for two different parameters on the same page,
distinguished only by subpage: subpage 0 (mode=0) carries rotary/slider params, subpage 1 (mode=1)
carries button params. A dispatcher that matches on `page + paramCol` alone will route an incoming
hardware change to the wrong `XpanderParam` roughly half the time on any page with this collision.

## Decision

`EditorTabComponent::onParameterChangedFromHardware`'s dispatch (via the pre-built `paramsByPage_` map)
matches BOTH `page + paramCol` AND `def.isButton == isButton`. Both flags must be correct for a given
`XpanderParam` definition in `XpanderData.h` for dispatch to land on the right parameter.

## Consequences

- Every new parameter added to `XpanderData.h` must set `isButton` correctly, and must be checked against
  the C# reference for whether its `paramCol` collides with a subpage-0 parameter on the same page.
- A parameter that's added with the wrong `isButton` flag will not throw or fail loudly — it will
  silently receive another parameter's hardware updates, which is a much harder bug to notice and
  diagnose than a crash.
