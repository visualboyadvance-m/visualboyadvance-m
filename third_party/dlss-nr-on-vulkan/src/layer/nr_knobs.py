"""Every knob the daemon has, described once.

The control tool, the panel and the documentation all render this table, so a knob
cannot exist in one and be missing from another — which has happened: `hold` went into
the daemon and never into `nr-ctl`, and nothing noticed until a test compared the two.

`summary` is what a panel shows next to the value. `detail` is what someone reading the
manual needs, and where a measurement exists it is quoted rather than described, because
several of these do not do what their names suggest.
"""
import collections

Knob = collections.namedtuple("Knob", "name label kind low high step default summary detail")

PROFILES = ("standard", "natural", "cinematic", "neutral")

KNOBS = (
    Knob(
        "render_scale", "render scale", "number", 0.05, 1.0, 0.05, 1.0,
        "the fraction of each side the network runs on",
        "The only knob that changes the frame rate. The network runs on a frame this "
        "much smaller, and what comes back is the *head* — the detail it drew — which is "
        "then scaled up and composed against the full-resolution original, so the game's "
        "own pixels are never resampled and only the synthesised part is interpolated. "
        "Cost follows the extent and nothing else: about 9 ms + 196 ms per megapixel "
        "of network extent on an Arc 140V. The extent is never below 320 on a side — the "
        "checkpoint's minimum — so small renders are padded up to it: at 512x288 every "
        "scale up to 0.62 runs the same 320x320 network as 0.35 does, with three times "
        "the real pixels in it. "
        "0.55 is the measured compromise, but the *sign* of its effect on quality depends "
        "on how dark the scene is rather than on the number: on a bright frame 0.55 adds "
        "15 % of local contrast to a kimono, on a dark crowd it takes 21 % away.",
    ),
    Knob(
        "profile", "profile", "choice", None, None, None, "standard",
        "which way to trade skin texture against speculars",
        "The three conditioning scalars the network is given. They are a clean monotone "
        "trade, not a quality ladder: everything the pass adds to skin texture it takes "
        "out of speculars and colour, and the profile chooses where on that curve to sit. "
        "`standard` is the better default for a game with bright, near-clipping skin. "
        "`cinematic` does not merely add less on such a frame — it *removes* detail, "
        "smoothing sand grain that `standard` keeps. `neutral` sets tone and structure to "
        "zero. Changing this costs a forward pass, unlike everything below it.",
    ),
    Knob(
        "intensity", "intensity", "number", 0.0, 2.0, 0.05, 1.0,
        "how far to go towards the model's picture, or past it",
        "Blends the model's answer against the source, per pixel where an interface mask "
        "supplies one. At 1 you get the model's picture; below it you get part of the way "
        "there; above it the blend extrapolates *past* the model, which the vendor's own "
        "panel allows to 2 and ships screenshots at 1.66. Post-network and free: sweeping "
        "it does not re-run anything.",
    ),
    Knob(
        "detail_strength", "detail strength", "number", 0.0, 2.0, 0.05, 1.0,
        "re-weights the high-frequency half of the change",
        "After the blend, the difference the pass made is split into bands and each is "
        "re-weighted. This is the fine half — pores, strands, grain. Away from 1 it costs "
        "a Gaussian over the whole frame. OpenCV provides a faster blur implementation; "
        "the cost depends on the machine and frame size.",
    ),
    Knob(
        "colour_strength", "colour strength", "number", 0.0, 2.0, 0.05, 1.0,
        "the low-frequency half — and it runs backwards from its name",
        "The coarse half of the same split: tone and colour rather than detail. **It does "
        "not restore colour.** 1.5 is the most aggressive of the measured settings — iris "
        "saturation 16.8 -> 8.8, half again below the default — because it scales the "
        "strength of the pass's colour term, not the colour that survives. The name "
        "invites the opposite reading and this project spent a measurement finding out.",
    ),
    Knob(
        "temporal", "temporal", "number", 0.0, 1.0, 0.05, 1.0,
        "how much of the model's own history gate to trust",
        "The previous output is fed back into the network's history channels, and the "
        "model's learned gate decides per pixel how much of it survives into this frame. "
        "This scales that gate. 0 turns the path off entirely, is bit-identical to drawing "
        "each frame alone, and clears the stored frame so switching back on cannot "
        "resurrect a stale one. Worth about 4 % of the frame time.",
    ),
    Knob(
        "hold", "hold", "number", 0.0, 1.0, 0.05, 1.0,
        "how hard to hold pixels the game did not move",
        "A floor under that gate, which the gate needs: the model is global, so on a frame "
        "where most things move it reads 0.12 even over pixels that did not move at all. "
        "Where the game handed back the same pixel the previous output is right for that "
        "pixel by construction, and this says so. It cannot ghost — the frame that changes "
        "a pixel is the frame that releases it. Together with `temporal` it takes the "
        "invention over still pixels from 3.25 levels of 255 to 0.87.",
    ),
    Knob(
        "cut_limit", "cut limit", "number", 0.0, 1.0, 0.01, 0.15,
        "the frame-to-frame change that counts as a new shot",
        "Mean absolute change between two presents above which the shot is taken to have "
        "cut and the history is thrown away. The gate rejects wrong history per pixel on "
        "its own, but it was characterised on a pan at full scale, so a whole-frame "
        "replacement — a round transition, a replay, a menu — is worth refusing outright.",
    ),
)

BY_NAME = {knob.name: knob for knob in KNOBS}
# What the daemon itself falls back to with no settings file. `test_toggle.py` checks
# these against its argument parser, because a panel that shows a wrong "current" value
# is worse than one that shows none.
DEFAULTS = {knob.name: knob.default for knob in KNOBS}

# The whole round trip, median of nine frames each, measured by `src/bench/live_rates.py`
# on 2026-09-24 — the daemon's own cost, with no game competing for the GPU. `nr-ctl rates`,
# the panel and the README all read this one table; the README's copy is generated from it
# by `src/tools/knob_doc.py`, because the hand-written one went two days out of date the
# moment the host passes moved to C and then stayed wrong for a week.
RATES = (
    (512, 288, 0.35, 35.7),
    (512, 288, 0.50, 37.5),
    (640, 360, 0.35, 37.5),
    (640, 360, 0.50, 39.2),
    (854, 480, 0.50, 50.4),
    (1024, 768, 0.55, 75.1),
    (1920, 1080, 0.55, 170.9),
)
RATES_MEASURED = "2026-09-24"
# What a reader of the table needs and the numbers cannot say. Empty when there is nothing.
RATES_NOTE = ("Medians of three runs with swap empty, which agreed within 4 %. On "
              "2026-09-23, with 5.5 GiB in zram and the kernel's memory-pressure figures "
              "rising, 1920x1080 ran anywhere from 322 to 463 ms: if that row is much slower "
              "for you, look at swap before anything else.")


def clamp(knob, value):
    """A value the daemon will accept, or None if this knob does not take numbers."""
    if knob.kind != "number":
        return None
    stepped = round(value / knob.step) * knob.step
    return round(min(max(stepped, knob.low), knob.high), 4)
