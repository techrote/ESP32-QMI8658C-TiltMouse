#!/usr/bin/env python3
"""Small synthetic policy comparison; no claims about physical pointer feel."""
import json, sys
from pathlib import Path
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
# Three same-button report intervals: backend rejects first two, accepts third.
candidates=[4,5,6]
drop=candidates[-1]
accum=sum(candidates)
latest=candidates[-1]
# At recovery t=16, keep candidates only from [8,16], with an 8-unit maximum age.
coalesce=sum(candidates[1:])
assert (drop,accum,latest,coalesce)==(6,15,6,11)
# Equal net count does not preserve intermediate direction or a button boundary.
boundary=[{'dx':8,'buttons':1},{'dx':-8,'buttons':0}]
assert sum(x['dx'] for x in boundary)==0
result={'label':'SYNTHETIC: integer counts, not measured cursor distance',
        'input_deltas':candidates,'backend_ready':[False,False,True],
        'drop_one_shot_emitted':drop,'unbounded_sum_emitted':accum,
        'latest_slot_emitted':latest,'bounded_sum_age_8_emitted':coalesce,
        'coalescing_assumptions':'Same valid button epoch, unsent deltas only; maximum age 8; bound not hit.',
        'cross_button_boundary':boundary,
        'naive_sum':0,'boundary_policy':'Do not coalesce across press/release; prioritize state and discard older movement.',
        'scope':'Illustrative policy arithmetic, not implementation of alternative schedulers.'}
(out/'policy_results.json').write_text(json.dumps(result,indent=2)+'\n')
