# ADR-008: Timbre Space self-contained PCA layout (no external ML/geometry dependency)

**Status**: Hardware-validated 2026-07-22 (`docs/TEST-PLAN.md` Session K — "I tested the TS function
and its operating as expected").

## Context

Timbre Space is a 2-D map of the patch library: each patch is projected to a point (position) and a
colour, and dragging in the map blends the nearby patches live to the hardware. The feature was
designed after reviewing the reference implementations that inspired it —
[`kijimi-babu-frik`](https://github.com/RitaAndAurora/kijimi-babu-frik) and
[`ddrm-jfsebastian`](https://github.com/RitaAndAurora/ddrm-jfsebastian) — which compute the layout with
the **tapkee** library (selectable t-SNE / MDS / PCA, backed by Eigen) and **delaunator-cpp** for the
triangulation used to interpolate a click into a blend.

Two properties mattered for this project that the reference's own choices don't guarantee:

- **Determinism.** A given library must always produce the same map — the same patch always lands in
  the same spot. t-SNE (the reference's *default* method) is stochastic; reproducing a layout requires
  pinning a seed and trusting cross-run/cross-platform stability that was never verified.
- **Dependency weight.** tapkee pulls in Eigen; delaunator-cpp is a full Delaunay triangulation
  library. XpandrLink's CMake build has no dependency of that class today, and the project has no
  established practice of vendoring third-party numerical/geometry libraries.

## Decision

Compute the layout with a **self-contained PCA** implementation (`TimbreSpaceEngine::computeLayout`,
`XpandrLink/Source/Data/TimbreSpaceEngine.h`): min-max normalize each continuous parameter dimension
across the patch set, then run a from-scratch cyclic **Jacobi eigensolver** on the resulting covariance
matrix — no RNG anywhere, so the result is bit-for-bit reproducible for a given input set. The top-2
eigenvectors give the 2-D position; the top-3 give an RGB colour.

Rather than a stored Delaunay mesh (`delaunator-cpp`), a query point's blend weights come from a
**self-contained 3-nearest-neighbour lookup** (`interpolationDataForPoint`): exact barycentric weights
if the point falls inside the triangle of its 3 nearest patches, otherwise inverse-distance weights over
those neighbours. This reproduces every observable property the reference's global mesh provided
(deterministic layout, exact reproduction at a patch's own point, smooth barycentric blending, a graceful
fallback outside the convex hull) without storing or maintaining a mesh at all.

Net result: **zero new third-party dependencies** for the entire feature — no Eigen, no tapkee, no
delaunator. See `docs/superpowers/specs/2026-07-21-timbre-space-design.md` for the full design
rationale and the planning-time refinement that dropped delaunator-cpp in favour of the self-contained
interpolation.

## Consequences

- The map is reproducible: re-opening SPACE with the same library always produces the same dots in the
  same places, which the design and code both rely on (no caller works around non-determinism).
- **This is the decision a future maintainer is most likely to "improve" away.** Swapping in a
  proper ML library (t-SNE for prettier cluster separation, or a real Delaunay implementation "for
  correctness") would silently reintroduce non-determinism and a heavy dependency this project
  deliberately avoided. Any such change should re-derive whether determinism is still required before
  proceeding — it is, per the design spec — and should update this ADR rather than silently drop the
  self-contained approach.
- The interpolation only ever considers the 3 nearest points, never a full triangulation — for very
  large or degenerate point distributions this can behave differently from a true Delaunay mesh at the
  boundary between two nearly-equidistant candidate triangles. Not observed as a problem in hardware
  testing; worth re-examining only if a future report specifically describes a discontinuous/jarring
  blend when crossing between distant patch clusters.
- Hardware-validated 2026-07-22 (see `docs/TEST-PLAN.md` Session K, `ROADMAP.md` closed-this-cycle
  entry): drag-to-blend, click-to-load-exact-patch, fast-drag flood safety (NFR-TIMING-07), Undo, and
  the scratchpad-99-only write guarantee (NFR-HW-01) all confirmed working on real hardware.
