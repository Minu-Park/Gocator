# 작업 지침

- Ultra concise (keywords only)
- Korean for chat, English for docs
- History: Manage history via git commits on EVERY turn. No separate LOG.md.
- Entry Point: `src/main.cpp` (CLI/UI branch)
- Diagnostics: Update `runDebug` in `src/cli_main.cpp`
- Data: Normalize 16-bit images for UI preview
- Architecture: `gocator_core` (no Qt) vs `src/ui_main.cpp` (Qt)

## 주요 마일스톤

- 2026-05-19: 통합 엔트리 포인트 및 진단 툴(`runDebug`) 구축
- 2026-05-19: 이미지/Surface 획득 및 16-bit 정규화 로직 검증
