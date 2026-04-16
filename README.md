<h1 align="center">
    <a href="https://openairinterface.org/"><img src="https://openairinterface.org/wp-content/uploads/2015/06/cropped-oai_final_logo.png" alt="OAI" width="550"></a>
</h1>

<p align="center">
    <a href="https://gitlab.eurecom.fr/oai/openairinterface5g/-/blob/master/LICENSE"><img src="https://img.shields.io/badge/license-OAI--Public--V1.1-blue" alt="License"></a>
    <a href="https://releases.ubuntu.com/22.04/"><img src="https://img.shields.io/badge/OS-Ubuntu22-Green" alt="Supported OS Ubuntu 22"></a>
    <a href="https://releases.ubuntu.com/24.04/"><img src="https://img.shields.io/badge/OS-Ubuntu24-Green" alt="Supported OS Ubuntu 24"></a>
    <a href="https://www.redhat.com/en/technologies/linux-platforms/enterprise-linux"><img src="https://img.shields.io/badge/OS-RHEL9-Green" alt="Supported OS RHEL9"></a>
    <a href="https://getfedora.org/en/workstation/"><img src="https://img.shields.io/badge/OS-Fedore41-Green" alt="Supported OS Fedora 41"></a>
</p>

<p align="center">
    <a href="https://gitlab.eurecom.fr/oai/openairinterface5g/-/releases"><img alt="GitLab Release (custom instance)" src="https://img.shields.io/gitlab/v/release/oai/openairinterface5g?gitlab_url=https%3A%2F%2Fgitlab.eurecom.fr&include_prereleases&sort=semver"></a>
</p>

<p align="center">
    <a href="https://jenkins-oai.eurecom.fr/job/RAN-Ubuntu18-Image-Builder/"><img src="https://img.shields.io/jenkins/build?jobUrl=https%3A%2F%2Fjenkins-oai.eurecom.fr%2Fjob%2FRAN-Ubuntu18-Image-Builder%2F&label=build-Ubuntu-x86%20Images"></a>
    <a href="https://jenkins-oai.eurecom.fr/job/RAN-RHEL8-Cluster-Image-Builder/"><img src="https://img.shields.io/jenkins/build?jobUrl=https%3A%2F%2Fjenkins-oai.eurecom.fr%2Fjob%2FRAN-RHEL8-Cluster-Image-Builder%2F&label=build-UBI-x86%20Images"></a>
    <a href="https://jenkins-oai.eurecom.fr/job/RAN-Ubuntu-ARM-Image-Builder/"><img src="https://img.shields.io/jenkins/build?jobUrl=https%3A%2F%2Fjenkins-oai.eurecom.fr%2Fjob%2FRAN-Ubuntu-ARM-Image-Builder%2F&label=build-Ubuntu-ARM%20Images"></a>
</p>

<p align="center">
  <a href="https://hub.docker.com/r/oaisoftwarealliance/oai-gnb"><img alt="Docker Pulls" src="https://img.shields.io/docker/pulls/oaisoftwarealliance/oai-gnb?label=gNB%20docker%20pulls"></a>
  <a href="https://hub.docker.com/r/oaisoftwarealliance/oai-nr-ue"><img alt="Docker Pulls" src="https://img.shields.io/docker/pulls/oaisoftwarealliance/oai-nr-ue?label=NR-UE%20docker%20pulls"></a>
  <a href="https://hub.docker.com/r/oaisoftwarealliance/oai-enb"><img alt="Docker Pulls" src="https://img.shields.io/docker/pulls/oaisoftwarealliance/oai-enb?label=eNB%20docker%20pulls"></a>
  <a href="https://hub.docker.com/r/oaisoftwarealliance/oai-lte-ue"><img alt="Docker Pulls" src="https://img.shields.io/docker/pulls/oaisoftwarealliance/oai-lte-ue?label=LTE-UE%20docker%20pulls"></a>
  <a href="https://hub.docker.com/r/oaisoftwarealliance/oai-nr-cuup"><img alt="Docker Pulls" src="https://img.shields.io/docker/pulls/oaisoftwarealliance/oai-nr-cuup?label=NR-CUUP%20docker%20pulls"></a>
</p>

# OpenAirInterface License #

 *  [OAI License Model](http://www.openairinterface.org/?page_id=101)
 *  [OAI License v1.1 on our website](http://www.openairinterface.org/?page_id=698)

It is distributed under **OAI Public License V1.1**.

The license information is distributed under [LICENSE](LICENSE) file in the same directory.

Please see [NOTICE](NOTICE.md) file for third party software that is included in the sources.

# Where to Start #

 *  [General overview of documentation](./doc/README.md)
 *  [The implemented features](./doc/FEATURE_SET.md)
 *  [System Requirements for Using OAI Stack](./doc/system_requirements.md)
 *  [How to build](./doc/BUILD.md)
 *  [How to run the modems](./doc/RUNMODEM.md)

Not all information is available in a central place, and information for
specific sub-systems might be available in the corresponding sub-directories.
To find all READMEs, this command might be handy:

```
find . -iname "readme*"
```

# RAN repository structure #

The OpenAirInterface (OAI) software is composed of the following parts: 

```
openairinterface5g
├── charts
├── ci-scripts        : Meta-scripts used by the OSA CI process. Contains also configuration files used day-to-day by CI.
├── CMakeLists.txt    : Top-level CMakeLists.txt for building
├── cmake_targets     : Build utilities to compile (simulation, emulation and real-time platforms), and generated build files.
├── common            : Some common OAI utilities, some other tools can be found at openair2/UTILS.
├── doc               : Documentation
├── docker            : Dockerfiles to build for Ubuntu and RHEL
├── executables       : Top-level executable source files (gNB, eNB, ...)
├── maketags          : Script to generate emacs tags.
├── nfapi             : (n)FAPI code for MAC-PHY interface
├── openair1          : Layer 1 (3GPP LTE Rel-10/12 PHY, NR Rel-15 PHY)
├── openair2          : Layer 2 (3GPP LTE Rel-10 MAC/RLC/PDCP/RRC/X2AP, LTE Rel-14 M2AP, NR Rel-15+ MAC/RLC/PDCP/SDAP/RRC/X2AP/F1AP/E1AP), E2AP
├── openair3          : Layer 3 (3GPP LTE Rel-10 S1AP/GTP, NR Rel-15 NGAP/GTP)
├── openshift         : OpenShift helm charts for some deployment options of OAI
├── radio             : Drivers for various radios such as USRP, AW2S, RFsim, 7.2 FHI, ...
├── targets           : Some configuration files; only historical relevance, and might be deleted in the future
└── tools             : Tools for use by the developers/ci machines: code analysis and formatting
```

# How to get support from the OAI Community # 

You can ask your question on the [mailing lists](https://gitlab.eurecom.fr/oai/openairinterface5g/-/wikis/MailingList).

Your email should contain below information:

- A clear subject in your email.
- For all the queries there should be [Query\] in the subject of the email and for problems there should be [Problem\].
- In case of a problem, add a small description.
- Do not share any photos unless you want to share a diagram.
- OAI gNB/DU/CU/CU-CP/CU-UP configuration file in `.conf` format only.
- Logs of OAI gNB/DU/CU/CU-CP/CU-UP in `.log` or `.txt` format only.
- In case your question is related to performance, include a small description of the machine (Operating System, Kernel version, CPU, RAM and networking card) and diagram of your testing environment.
- Known/open issues are present on [GitLab](https://gitlab.eurecom.fr/oai/openairinterface5g/-/issues), so keep checking.

Always remember a structured email will help us understand your issues quickly.

# OAIBOX-DU Symbol-Level Beam Scheduling Port Note #

This repository contains a backport of the MAC-layer symbol-level beam ID control scheduling changes from
`beam_symbol_switch_2026_w15` into `oaibox-du` (based on the older `2026.w07` line).

The goal of this port is to let MAC reserve beam resources with symbol granularity, instead of assuming that
one beam decision always occupies the whole slot. In practice, this matters because the following can now be
handled independently inside the same slot when resources do not overlap in time:

- PDCCH/DCI beam reservation
- PDSCH/PUSCH beam reservation
- PRACH reservation
- Msg2/Msg3/Msg4 random-access related reservations
- PUCCH/SRS/CSI-RS reservations

The port keeps the existing `oaibox-du` specific behavior intact where possible, especially:

- the numeric `set_analog_beamforming` configuration style
- NSSAI/slice-aware RB range handling in DL/UL scheduling
- existing OAIBOX-specific scheduler tuning and logging

## Configuration Semantics ##

The beam scheduler is enabled through the existing MACRLC configuration block.

- `set_analog_beamforming = 2` means LoPHY beam index mode in `oaibox-du`
- `beam_duration > 0` means slot-level allocation
- `beam_duration < 0` means symbol-level allocation, and `abs(beam_duration)` is the symbol group size
- `beam_duration = 0` is invalid and rejected during configuration
- `beams_per_period` is the number of parallel beam "planes" the scheduler can use for the same time region

Example:

```conf
MACRLCs = ({
  set_analog_beamforming = 2;
  beam_duration = -7;
  beams_per_period = 2;
  beam_weights = [0];
});
```

With `beam_duration = -7`, one slot is split into two symbol groups:

- group 0: symbols 0..6
- group 1: symbols 7..13

This allows, for example, an early-symbol DCI reservation and a later-symbol PDSCH/PUSCH reservation to be
tracked separately.

## What Was Ported ##

The main functional changes are:

- `openair2/LAYER2/NR_MAC_gNB/nr_mac_gNB.h`
- `openair2/LAYER2/NR_MAC_gNB/mac_proto.h`
- `openair2/LAYER2/NR_MAC_gNB/config.c`
- `openair2/LAYER2/NR_MAC_gNB/gNB_scheduler.c`
- `openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_primitives.c`
- `openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_bch.c`
- `openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_dlsch.c`
- `openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_ulsch.c`
- `openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_RA.c`
- `openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_uci.c`
- `openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_srs.c`
- `openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_phytest.c`
- `openair2/GNB_APP/MACRLC_nr_paramdef.h`
- `openair2/GNB_APP/gnb_config.c`

The high-level behavior is now:

- beam reservation is performed with `(frame, slot, start_symbol, nb_symbols, beam_id)`
- DCI/PDCCH beam reservation is separated from PDSCH/PUSCH beam reservation
- retransmission helpers return enough information to undo only the newly reserved beam groups
- RA scheduling reserves beams using the real time-domain allocation of PRACH, Msg2, Msg3 and Msg4
- long PRACH formats reserve beam occupancy across all affected slots

## `nr_mac_gNB.h` Design Intent ##

The most important structural change is in `NR_beam_info_t`.

Previous behavior assumed a 2D allocation model:

- one axis for "beam period"
- one axis for "time bucket"

That was enough for slot-level beam scheduling, but it was not enough once one slot had to be split into
multiple independently reservable symbol groups.

The new structure is:

```c
typedef struct {
  int16_t ***beam_allocation;
  int beam_slot_duration;
  int beam_symbol_duration;
  int beams_per_period;
  int beam_allocation_size[2];
  nr_beam_mode_t beam_mode;
} NR_beam_info_t;
```

The intended meaning is:

- `beam_allocation[beam_period_idx][slot_group_idx][symbol_group_idx]`
- stored value: requested beam ID
- stored value `-1`: this entry is free

So `beam_allocation` is no longer "a list of beams per period" only; it is a 3D occupancy table.

The three axes are:

- `beam_period_idx`
  This selects one of the `beams_per_period` parallel beam allocation planes.
- `slot_group_idx`
  This is the time axis across slots. It is grouped by `beam_slot_duration`.
- `symbol_group_idx`
  This is the sub-slot axis inside one slot. It is grouped by `beam_symbol_duration`.

The reason `beam_allocation_size` is now `int beam_allocation_size[2]` is exactly to describe the sizes of the
last two axes of that 3D table:

- `beam_allocation_size[0]`
  Number of slot groups on the slot axis.
  Computed as `20ms_window_in_slots / beam_slot_duration`.
  In the current implementation, the window is `slots_per_frame * 2`, following the original 20 ms periodic
  allocation model used by the source branch.
- `beam_allocation_size[1]`
  Number of symbol groups inside one slot.
  Computed as `14 / symbol_group_size`.
  Slot-level mode uses `1`, because the whole slot is one group.

Concrete examples:

- slot-level mode with `beam_duration = 1`
  `beam_slot_duration = 1`, `beam_symbol_duration = 0`, `beam_allocation_size[1] = 1`
- symbol-level mode with `beam_duration = -7`
  `beam_slot_duration = 1`, `beam_symbol_duration = 7`, `beam_allocation_size[1] = 2`
- symbol-level mode with `beam_duration = -2`
  `beam_slot_duration = 1`, `beam_symbol_duration = 2`, `beam_allocation_size[1] = 7`

`beam_allocation_size[2]` is admittedly not a very descriptive name by itself. It was kept in array form to
stay close to the source branch and to keep the allocation/reset loops generic with minimal divergence during
the backport. The semantic interpretation in this repository is:

- `beam_allocation_size[0] = slot-axis size`
- `beam_allocation_size[1] = symbol-axis size`

If this area is refactored later, a more explicit representation such as:

- `beam_slot_axis_size`
- `beam_symbol_axis_size`

would likely improve readability without changing the behavior.

Two related changes are also important:

- `NR_beam_alloc_t.new_beam` is now a bitmap, not a boolean
- each bit in `new_beam` corresponds to one `symbol_group_idx`

This lets the scheduler free only the symbol groups that were newly reserved by the current allocation attempt.
That is why reset paths can now undo partial reservations safely, instead of dropping an entire slot-wide beam
state.

## Scheduler Behavior After the Port ##

After this port, the practical scheduling rule is:

- reserve the control-region beam using the real PDCCH start symbol and duration
- reserve the data-region beam using the real PDSCH/PUSCH/PUCCH/SRS/CSI-RS start symbol and duration
- allow both to coexist only if they do not conflict in the same occupancy table entry

This is especially visible in RA:

- PRACH uses the PRACH symbol span, and long PRACH occupies all affected slots
- Msg3 uses UL TDA symbols for PUSCH and PDCCH symbols for its DCI separately
- Msg2 and Msg4/MsgB reserve DCI and PDSCH beams separately

## Tests and Validation ##

The following validation artifacts were added:

- `openair2/LAYER2/NR_MAC_gNB/tests/test_beam_symbol_alloc.c`
- `openair2/LAYER2/NR_MAC_gNB/tests/run_beam_symbol_test.sh`
- `ci-scripts/conf_files/gnb.band78.106prb.rfsim.beam-symbol.conf`

Validation status for this port:

- self-contained unit test for beam allocation/reset logic: added and passing
- shell syntax check for RFsim smoke script: passing
- repository-wide build in `oaibox-du`: confirmed successful after the port
- RFsim smoke-test script: added for follow-up runtime validation

The unit test covers:

- slot-level regression behavior
- symbol-level allocation behavior
- non-overlapping symbol-group coexistence
- reset of only the symbol groups newly reserved by the current allocation
- DCI/PDSCH split-style allocation behavior

## Notes for Future Maintainers ##

When working on this area, the most important thing to remember is:

- a "beam allocation" is no longer synonymous with "this whole slot uses that beam"

Any new scheduler path that reserves radio resources should pass the real time-domain allocation to
`beam_allocation_procedure()`. If a path falls back to slot-only assumptions, it can silently reintroduce false
conflicts or leak beam reservations across unrelated symbol regions.
