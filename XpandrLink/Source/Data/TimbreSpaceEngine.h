/*
  TimbreSpaceEngine.h
  Deterministic 2-D layout of a patch set via PCA (self-contained Jacobi
  eigensolver), plus a 3-nearest barycentric / inverse-distance interpolation
  query. Pure logic, header-only. See docs/superpowers/specs/2026-07-21-timbre-space-design.md.
*/
#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include "TimbreTypes.h"
#include "XpanderData.h"
#include "PatchRandomizer.h"   // isContinuous

class TimbreSpaceEngine
{
public:
    void computeLayout(const std::vector<TimbrePatch>& input)
    {
        points_.clear();
        patches_.clear();

        // 1. Drop "empty" patches (all params zero / none set).
        for (const auto& p : input)
        {
            long long sum = 0;
            for (const auto& kv : p.params) sum += kv.second;
            if (sum > 0) patches_.push_back(p);
        }
        const int N = (int)patches_.size();
        if (N == 0) return;

        // 2. Build the continuous-only feature matrix (N rows x Dfull cols).
        std::vector<const XpanderParam*> dims;
        for (const auto& def : XpanderParams)
            if (PatchRandomizer::isContinuous(def)) dims.push_back(&def);

        std::vector<std::vector<double>> X((size_t)N);
        for (int i = 0; i < N; ++i)
        {
            X[(size_t)i].resize(dims.size());
            for (size_t d = 0; d < dims.size(); ++d)
            {
                auto it = patches_[(size_t)i].params.find(dims[d]->id);
                X[(size_t)i][d] = (it != patches_[(size_t)i].params.end())
                                  ? (double)it->second : (double)dims[d]->min;
            }
        }

        // 3. Per-dimension min-max normalize; drop zero-variance dims.
        std::vector<int> keep;
        for (size_t d = 0; d < dims.size(); ++d)
        {
            double mn = X[0][d], mx = X[0][d];
            for (int i = 1; i < N; ++i) { mn = std::min(mn, X[(size_t)i][d]); mx = std::max(mx, X[(size_t)i][d]); }
            if (mx - mn > 1.0e-12) keep.push_back((int)d);
        }
        const int D = (int)keep.size();

        std::vector<std::vector<double>> F((size_t)N, std::vector<double>((size_t)std::max(D,1), 0.0));
        for (int i = 0; i < N; ++i)
            for (int c = 0; c < D; ++c)
            {
                int d = keep[(size_t)c];
                double mn = X[0][(size_t)d], mx = X[0][(size_t)d];
                for (int j = 1; j < N; ++j) { mn = std::min(mn, X[(size_t)j][(size_t)d]); mx = std::max(mx, X[(size_t)j][(size_t)d]); }
                F[(size_t)i][(size_t)c] = (X[(size_t)i][(size_t)d] - mn) / (mx - mn);
            }

        // Degenerate: no variance anywhere -> collapse everything to the centre.
        if (D == 0)
        {
            for (int i = 0; i < N; ++i)
                points_.push_back({ patches_[(size_t)i].libraryId, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f });
            return;
        }

        // 4. Mean-center, covariance C = Fc^T Fc / (N-1)  (D x D).
        std::vector<double> mean((size_t)D, 0.0);
        for (int i = 0; i < N; ++i) for (int c = 0; c < D; ++c) mean[(size_t)c] += F[(size_t)i][(size_t)c];
        for (int c = 0; c < D; ++c) mean[(size_t)c] /= (double)N;
        for (int i = 0; i < N; ++i) for (int c = 0; c < D; ++c) F[(size_t)i][(size_t)c] -= mean[(size_t)c];

        std::vector<double> C((size_t)D * (size_t)D, 0.0);
        const double denom = (N > 1) ? (double)(N - 1) : 1.0;
        for (int a = 0; a < D; ++a)
            for (int b = a; b < D; ++b)
            {
                double s = 0.0;
                for (int i = 0; i < N; ++i) s += F[(size_t)i][(size_t)a] * F[(size_t)i][(size_t)b];
                s /= denom;
                C[(size_t)a * (size_t)D + (size_t)b] = s;
                C[(size_t)b * (size_t)D + (size_t)a] = s;
            }

        // 5. Eigendecompose (symmetric Jacobi); sort components by eigenvalue desc.
        std::vector<double> evals, evecs;
        jacobiEigen(C, D, evals, evecs);
        std::vector<int> order((size_t)D);
        for (int i = 0; i < D; ++i) order[(size_t)i] = i;
        std::sort(order.begin(), order.end(),
                  [&](int a, int b){ return evals[(size_t)a] > evals[(size_t)b]; });

        auto axisColumn = [&](int rank) { return (rank < D) ? order[(size_t)rank] : -1; };
        auto project = [&](int i, int rank) -> double
        {
            int col = axisColumn(rank);
            if (col < 0) return 0.0;
            double v = 0.0;
            for (int d = 0; d < D; ++d) v += F[(size_t)i][(size_t)d] * evecs[(size_t)d * (size_t)D + (size_t)col];
            return v;
        };

        // 6. Position = top-2 components; colour = top-3.
        std::vector<double> px((size_t)N), py((size_t)N), cr((size_t)N), cg((size_t)N), cb((size_t)N);
        for (int i = 0; i < N; ++i)
        {
            px[(size_t)i] = project(i, 0); py[(size_t)i] = project(i, 1);
            cr[(size_t)i] = project(i, 0); cg[(size_t)i] = project(i, 1); cb[(size_t)i] = project(i, 2);
        }
        normalize01(px); normalize01(py);
        normalize01(cr); normalize01(cg); normalize01(cb);

        for (int i = 0; i < N; ++i)
            points_.push_back({ patches_[(size_t)i].libraryId,
                                (float)px[(size_t)i], (float)py[(size_t)i],
                                (float)cr[(size_t)i], (float)cg[(size_t)i], (float)cb[(size_t)i] });
    }

    const std::vector<TimbrePoint>& getPoints()  const { return points_; }
    const std::vector<TimbrePatch>& getPatches() const { return patches_; }
    bool isEmpty() const { return points_.empty(); }

    // Task 3 fills this in.
    std::vector<WeightedPatch> interpolationDataForPoint(float x, float y) const;

private:
    static void normalize01(std::vector<double>& v)
    {
        if (v.empty()) return;
        double mn = v[0], mx = v[0];
        for (double d : v) { mn = std::min(mn, d); mx = std::max(mx, d); }
        double range = mx - mn;
        if (range < 1.0e-12) { for (double& d : v) d = 0.5; return; }
        for (double& d : v) d = (d - mn) / range;
    }

    // Symmetric eigendecomposition via cyclic Jacobi. `a` is n x n row-major.
    // Result: evals (diagonal, unsorted); evecs columns are the eigenvectors (n x n row-major).
    static void jacobiEigen(std::vector<double> a, int n,
                            std::vector<double>& evals, std::vector<double>& evecs)
    {
        evecs.assign((size_t)n * (size_t)n, 0.0);
        for (int i = 0; i < n; ++i) evecs[(size_t)i * (size_t)n + (size_t)i] = 1.0;

        for (int sweep = 0; sweep < 100; ++sweep)
        {
            double off = 0.0;
            for (int p = 0; p < n; ++p)
                for (int q = p + 1; q < n; ++q)
                    off += a[(size_t)p * (size_t)n + (size_t)q] * a[(size_t)p * (size_t)n + (size_t)q];
            if (off < 1.0e-20) break;

            for (int p = 0; p < n; ++p)
            for (int q = p + 1; q < n; ++q)
            {
                double apq = a[(size_t)p * (size_t)n + (size_t)q];
                if (std::abs(apq) < 1.0e-30) continue;
                double app = a[(size_t)p * (size_t)n + (size_t)p];
                double aqq = a[(size_t)q * (size_t)n + (size_t)q];
                double phi = 0.5 * std::atan2(2.0 * apq, aqq - app);
                double c = std::cos(phi), s = std::sin(phi);

                // A <- A * J  (rotate columns p,q)
                for (int k = 0; k < n; ++k)
                {
                    double akp = a[(size_t)k * (size_t)n + (size_t)p];
                    double akq = a[(size_t)k * (size_t)n + (size_t)q];
                    a[(size_t)k * (size_t)n + (size_t)p] = c * akp - s * akq;
                    a[(size_t)k * (size_t)n + (size_t)q] = s * akp + c * akq;
                }
                // A <- J^T * A  (rotate rows p,q)
                for (int k = 0; k < n; ++k)
                {
                    double apk = a[(size_t)p * (size_t)n + (size_t)k];
                    double aqk = a[(size_t)q * (size_t)n + (size_t)k];
                    a[(size_t)p * (size_t)n + (size_t)k] = c * apk - s * aqk;
                    a[(size_t)q * (size_t)n + (size_t)k] = s * apk + c * aqk;
                }
                // V <- V * J
                for (int k = 0; k < n; ++k)
                {
                    double vkp = evecs[(size_t)k * (size_t)n + (size_t)p];
                    double vkq = evecs[(size_t)k * (size_t)n + (size_t)q];
                    evecs[(size_t)k * (size_t)n + (size_t)p] = c * vkp - s * vkq;
                    evecs[(size_t)k * (size_t)n + (size_t)q] = s * vkp + c * vkq;
                }
            }
        }
        evals.assign((size_t)n, 0.0);
        for (int i = 0; i < n; ++i) evals[(size_t)i] = a[(size_t)i * (size_t)n + (size_t)i];
    }

    std::vector<TimbrePoint> points_;
    std::vector<TimbrePatch> patches_;
};

inline std::vector<WeightedPatch> TimbreSpaceEngine::interpolationDataForPoint(float x, float y) const
{
    const int N = (int)points_.size();
    if (N == 0) return {};

    // Nearest up to 3 points by squared distance.
    std::vector<std::pair<double,int>> d;
    d.reserve((size_t)N);
    for (int i = 0; i < N; ++i)
    {
        double dx = (double)points_[(size_t)i].x - (double)x;
        double dy = (double)points_[(size_t)i].y - (double)y;
        d.push_back({ dx*dx + dy*dy, i });
    }
    std::sort(d.begin(), d.end(), [](auto& a, auto& b){ return a.first < b.first; });
    const int k = std::min(3, N);

    // Exactly on a point -> that point wins.
    if (d[0].first < 1.0e-12)
        return { { d[0].second, 1.0f } };

    // Try barycentric inside the triangle of the 3 nearest.
    if (k == 3)
    {
        const auto& a = points_[(size_t)d[0].second];
        const auto& b = points_[(size_t)d[1].second];
        const auto& c = points_[(size_t)d[2].second];
        double det = (double)(b.y - c.y) * (a.x - c.x) + (double)(c.x - b.x) * (a.y - c.y);
        if (std::abs(det) > 1.0e-12)
        {
            double w0 = ((double)(b.y - c.y) * (x - c.x) + (double)(c.x - b.x) * (y - c.y)) / det;
            double w1 = ((double)(c.y - a.y) * (x - c.x) + (double)(a.x - c.x) * (y - c.y)) / det;
            double w2 = 1.0 - w0 - w1;
            const double eps = -1.0e-6;
            if (w0 >= eps && w1 >= eps && w2 >= eps)
            {
                double c0 = std::max(0.0, w0), c1 = std::max(0.0, w1), c2 = std::max(0.0, w2);
                double s = c0 + c1 + c2;
                if (s > 0.0)
                    return { { d[0].second, (float)(c0/s) },
                             { d[1].second, (float)(c1/s) },
                             { d[2].second, (float)(c2/s) } };
            }
        }
    }

    // Fallback: inverse-distance over the k nearest.
    std::vector<WeightedPatch> out;
    double s = 0.0;
    const double e2 = 1.0e-9;
    for (int i = 0; i < k; ++i) { double wgt = 1.0 / (std::sqrt(d[(size_t)i].first) + e2); out.push_back({ d[(size_t)i].second, (float)wgt }); s += wgt; }
    if (s > 0.0) for (auto& w : out) w.weight = (float)((double)w.weight / s);
    return out;
}
