# Alert rules – Log Status

An alert rule can look into the logbook before it alerts. The *Log Status*
box of the rule reads as one sentence:

    [x] Log Status:  I need this [ New Entity | New Band | New Mode | New Band & Mode ]
                     until it is       [ Worked | Confirmed ]

The rule alerts as long as the sentence is true. With the box unchecked the
log is not consulted and the rule alerts whatever is already worked or
confirmed.

## I need this …

| Choice | The rule alerts while the country is not yet worked / confirmed … |
|---|---|
| **New Entity** | on any band, in any mode |
| **New Band** | on the band of the spot, in any mode |
| **New Mode** | in the mode of the spot (CW, Phone, Digital, FTx), on any band |
| **New Band & Mode** | on the band of the spot in the mode of the spot (this is how the DX Cluster window colours its rows) |

## … until it is

| Choice | Meaning |
|---|---|
| **Worked** | at least one QSO is in the log |
| **Confirmed** | a QSL is received |

Which QSLs count as a confirmation (LoTW, eQSL, paper) is selected in
*Settings → Sync & QSL → DXCC Status*. The same selection drives the colours in
the DX Cluster window, so alerts and colours always agree.

## Examples

The log contains Jamaica (6Y) on 15 m SSB, not confirmed. A spot for 6Y9A on
10 m CW arrives.

| Rule | Alert? | Why |
|---|---|---|
| New Entity until Confirmed | yes | Jamaica is not confirmed on any band |
| New Entity until Worked | no | Jamaica is already in the log |
| New Band until Confirmed | yes | nothing on 10 m yet |
| New Mode until Worked | yes | Jamaica never worked in CW |
| New Band & Mode until Worked | yes | nothing on 10 m CW yet |

Once a LoTW confirmation for the 15 m SSB QSO arrives, the first rule goes
quiet; the third and fourth still alert.

Satellite QSOs are tracked per entity only, so when the New Contact widget is
in satellite mode every choice behaves like *New Entity*.
