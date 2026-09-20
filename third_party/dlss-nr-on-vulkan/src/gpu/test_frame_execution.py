#!/usr/bin/env python3
"""Exact full-frame equivalence, cold capture, resolution eviction and recovery."""
import pathlib
import sys
import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'ref'))
import nr_frame


def main():
    if not nr_frame.WEIGHTS.exists():
        print('frame execution: skipped (local logical weights missing)')
        return
    backend = nr_frame.ResidentBackend()
    rt = backend.runtime
    begin = rt.begin
    recordings = []

    def counted_begin():
        recordings.append(1)
        return begin()

    rt.begin = counted_begin
    saved = None
    previous = None
    for height, width in ((320, 320), (384, 320), (320, 320)):
        rng = np.random.default_rng(28)
        color = rng.random((height, width, 3), dtype=np.float32)
        features = nr_frame.make_features(color, **nr_frame.PROFILES['standard'])
        frame = backend.frame(*features.shape[:2])
        if previous is not None:
            assert previous._closed and not previous._graphs and not previous._buffers
        # The first-ever run must work without warming buffers in block mode.
        recordings.clear()
        head = frame.run(features, execution='replay')
        assert len(recordings) == 1
        recordings.clear()
        np.testing.assert_array_equal(frame.run(features, execution='replay'), head)
        assert not recordings
        recordings.clear()
        np.testing.assert_array_equal(frame.run(features, execution='block'), head)
        # Block mode submits once per block. With the card's memory unmapped it also cannot
        # keep its host-copy reference for the five skips, which become device copies with
        # a recording each — the same bytes, five more submissions.
        expected = 78 if rt.staging else 73
        assert len(recordings) == expected, (len(recordings), expected)
        recordings.clear()
        np.testing.assert_array_equal(frame.run(features, execution='single'), head)
        assert len(recordings) == 1
        changed = features.copy()
        changed[..., 4:7] *= np.float32(0.75)
        changed_head = frame.run(changed, execution='block')
        assert not np.array_equal(head, changed_head)
        np.testing.assert_array_equal(frame.run(changed, execution='replay'), changed_head)
        # A graph captured for generic shaders must coexist with specialized ones.
        rt.specialize(0)
        np.testing.assert_array_equal(frame.run(features, execution='replay'), head)
        rt.specialize(7)
        np.testing.assert_array_equal(frame.run(features, execution='replay'), head)
        if height == width == 320:
            if saved is None:
                saved = head
            else:
                np.testing.assert_array_equal(head, saved)
        previous = frame
    # A recording failure must not leave the singleton runtime stuck recording.
    record_block = backend._module.R.record_block

    def fail(*args, **kwargs):
        raise ValueError('injected recording failure')

    backend._module.R.record_block = fail
    try:
        try:
            frame.run(features, execution='single')
        except ValueError as error:
            assert str(error) == 'injected recording failure'
        else:
            raise AssertionError('expected recording failure')
    finally:
        backend._module.R.record_block = record_block
    np.testing.assert_array_equal(frame.run(features, execution='replay'), saved)
    backend.close()
    try:
        frame.run(features)
    except RuntimeError as error:
        assert 'closed' in str(error)
    else:
        raise AssertionError('closed frame accepted')
    print('frame execution: exact modes, 73 -> 1 submissions, cold capture, eviction, recovery OK')


if __name__ == '__main__':
    main()
