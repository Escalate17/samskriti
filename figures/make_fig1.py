"""Regenerate the architecture diagram.

The version in the published paper has colliding text — box titles are drawn over
their own body text. This redraws the same architecture with the layout fixed.
Content is unchanged; only spacing and placement differ.

Run: python figures/make_fig1.py
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Rectangle

plt.rcParams["font.family"] = "serif"
plt.rcParams["font.serif"] = ["DejaVu Serif", "Times New Roman"]

FIG_W, FIG_H = 12, 7.2
fig, ax = plt.subplots(figsize=(FIG_W, FIG_H))
ax.set_xlim(0, 100)
ax.set_ylim(0, 60)
ax.axis("off")

GROUP = dict(facecolor="none", edgecolor="#cccccc", linewidth=1.4, zorder=1)
BOX = dict(boxstyle="round,pad=0.6,rounding_size=1.2", facecolor="white",
           edgecolor="black", linewidth=2.0, zorder=3)


def group(x, y, w, h, label):
    ax.add_patch(Rectangle((x, y), w, h, **GROUP))
    ax.text(x + w / 2, y + h + 1.4, label, ha="center", va="bottom",
            fontsize=15, fontweight="bold", color="#888888", zorder=2)


def box(cx, cy, w, h, title, body):
    ax.add_patch(FancyBboxPatch((cx - w / 2, cy - h / 2), w, h, **BOX))
    # title sits in the upper half, body in the lower half — the collision in the
    # original came from drawing both at the same anchor
    ax.text(cx, cy + h * 0.20, title, ha="center", va="center",
            fontsize=13, fontweight="bold", zorder=4)
    ax.text(cx, cy - h * 0.22, body, ha="center", va="center",
            fontsize=10.5, linespacing=1.5, zorder=4)


def link(p, q):
    ax.plot([p[0], q[0]], [p[1], q[1]], color="black", linewidth=1.8, zorder=2)


# ── groups ──────────────────────────────────────────────────────────────────
group(4, 27, 42, 27, "Per-Soul State")
group(54, 27, 42, 27, "Population Aggregates")
group(4, 5, 92, 18, "Inter-Soul Dynamics")

# ── per-soul state ──────────────────────────────────────────────────────────
esv = (25, 46)
box(*esv, 36, 10,
    "Emotional State Vector",
    "23-dimensional continuous state,\nper-rasa decay each tick")

sam = (25, 33.5)
box(*sam, 36, 10,
    "Samskara Memory",
    "Event-driven impressions,\nlong-term retention")

# ── population aggregates ───────────────────────────────────────────────────
pop = (75, 41)
box(*pop, 36, 12,
    "Population-Level Observables",
    "Aggregate civilization metrics\nacross the soul population")

# ── inter-soul dynamics ─────────────────────────────────────────────────────
cpl = (20, 13)
box(*cpl, 28, 10.5,
    "Continuous Coupling",
    "Proximity-based interaction,\nrasa exchange")

bnd = (50, 13)
box(*bnd, 26, 10.5,
    "Bond Network",
    "Dynamic link formation,\nstrength updates")

rep = (80, 13)
box(*rep, 28, 10.5,
    "Reproduction & Inheritance",
    "Generational transfer,\nchild trait allocation")

# ── connections ─────────────────────────────────────────────────────────────
link((43, 46), (57, 43))       # emotional state -> population observables
link((25, 28.4), (25, 18.2))     # samskara -> continuous coupling
link((34, 13), (37, 13))         # coupling -> bond network
link((63, 13), (66, 13))         # bond network -> reproduction
link((55, 18.2), (70, 35.2))     # bond network -> population observables
link((82, 18.2), (79, 35.2))     # reproduction -> population observables

fig.tight_layout(pad=0.4)
out = __file__.replace("make_fig1.py", "fig1_architecture.png")
fig.savefig(out, dpi=160, facecolor="white", bbox_inches="tight")
print(f"wrote {out}")
