# First-run review of 2026-11-10: status

The live ledger for [the review](README.md). Record each resolution here in the commit that makes it: the commit and its evidence, the story the owner made of it, or the reason it was declined. The row leaves [TODO.md](../../../TODO.md) in the same commit. A finding is a question until its fix proves it.

| id | Finding | State |
|---|---|---|
| `REV#106` | [A 32-bit prompt builds the starter silently, and the next x64 build fails with a linker flood](FINDINGS.md#rev106) | Open |
| `REV#107` | [Install the tools sends deep-folder readers to a subst drive, where the starter's extract fails](FINDINGS.md#rev107) | Done. Keeping a deep folder now applies only to a project that has to stay in a deep folder. It sends starter readers to C:\dev, puts core.longpaths behind "if you use Git", keeps P:\ for the Git route only, and warns that Windows' tar cannot extract at a subst drive's root. The starter page says its boxes use C:\dev as written and names the Can't create message with its cause. |
| `REV#108` | [Move a starter project to a newer Labrador carries over only the game folder](FINDINGS.md#rev108) | Open |
| `REV#109` | [Troubleshooting has no entry for several messages a first-timer really gets](FINDINGS.md#rev109) | Open |
| `REV#110` | [Install the tools: the installer's own screens are unwritten and nothing says where a starter reader is finished](FINDINGS.md#rev110) | Open |
| `REV#111` | [No check of the machine, and no total cost, before the reader starts a multi-gigabyte install](FINDINGS.md#rev111) | Open |
| `REV#112` | [Smart App Control or Defender blocking the reader's own MyGame.exe is not covered](FINDINGS.md#rev112) | Open |
| `REV#113` | [On a Windows that is not in English the quoted messages do not appear](FINDINGS.md#rev113) | Open |
| `REV#114` | [Coming back after a Visual Studio update, and removing everything, are not covered](FINDINGS.md#rev114) | Open |
| `REV#115` | [The release page invites two wrong downloads, and extracting again silently overwrites an edited project](FINDINGS.md#rev115) | Open |
| `REV#116` | [Links and next-page order cross from the starter route into the Git route, and the guides index is one ungrouped grid](FINDINGS.md#rev116) | Open |
| `REV#117` | [Day-two guides interact in ways no page states](FINDINGS.md#rev117) | Open |
| `REV#118` | [The starter ZIP is the whole engine repository, and its own files describe themselves as the minimal sample](FINDINGS.md#rev118) | Open |
| `REV#119` | [No page reads the starter's own files at a beginner's level, and Get Started has no glossary](FINDINGS.md#rev119) | Open |
| `REV#120` | [The one specific command prompt is the costliest concept on the route, and the starter ships nothing that removes it](FINDINGS.md#rev120) | Open |
| `REV#121` | [The first configure downloads a CMake and a PowerShell although Visual Studio has just installed both](FINDINGS.md#rev121) | Open |

Evidence this review added to `REV#62`, `REV#63` and `REV#66` is under [its own heading](FINDINGS.md#evidence-added-to-existing-rows); those rows' states stay in [the ledger of the review of 2026-11-10](../2026-11-10/STATUS.md).
