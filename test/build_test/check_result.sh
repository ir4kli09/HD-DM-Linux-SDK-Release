#!/bin/sh

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color
ESPDI_DIR=./eSPDI
CONSOLE_TESTER_DIR=./console_tester
QT_DIR=./bin

echo ""
echo "========================================"
echo "Build eSPDI library result"
echo "========================================"
printf "libeSPDI_X86_64 Build : "
[ -f "${ESPDI_DIR}/libeSPDI_X86_64.so" ] && echo "${GREEN}Successed${NC}" || echo "${RED}Failed${NC}"

printf "libeSPDI_NVIDIA_64 Build : "
[ -f "${ESPDI_DIR}/libeSPDI_NVIDIA_64.so" ] && echo "${GREEN}Successed${NC}" || echo "${RED}Failed${NC}"

printf "libeSPDI_hypatia_32 Build : "
[ -f "${ESPDI_DIR}/libeSPDI_hypatia_32.so" ] && echo "${GREEN}Successed${NC}" || echo "${RED}Failed${NC}"

printf "libeSPDI_TI_32 Build : "
[ -f "${ESPDI_DIR}/libeSPDI_TI_32.so" ] && echo "${GREEN}Successed${NC}" || echo "${RED}Failed${NC}"

echo "========================================"

echo ""
echo "========================================"
echo "Build console tester result"
echo "========================================"

printf "test_x86 Build : "
[ -f "${CONSOLE_TESTER_DIR}/test_x86" ] && echo "${GREEN}Successed${NC}" || echo "${RED}Failed${NC}"

printf "test_arm64_tx2 Build : "
[ -f "${CONSOLE_TESTER_DIR}/test_arm64_tx2" ] && echo "${GREEN}Successed${NC}" || echo "${RED}Failed${NC}"

printf "test_armhf Build : "
[ -f "${CONSOLE_TESTER_DIR}/test_armhf" ] && echo "${GREEN}Successed${NC}" || echo "${RED}Failed${NC}"

printf "test_armhf_ti Build : "
[ -f "${CONSOLE_TESTER_DIR}/test_armhf_ti" ] && echo "${GREEN}Successed${NC}" || echo "${RED}Failed${NC}"
echo "========================================"

echo ""
echo "========================================"
echo "Build QT application result"
echo "========================================"

printf "DMPreview_X86 Build : "
[ -f "${QT_DIR}/DMPreview_X86" ] && echo "${GREEN}Successed${NC}" || echo "${RED}Failed${NC}"

printf "DMPreview_TX2 Build : "
[ -f "${QT_DIR}/DMPreview_TX2" ] && echo "${GREEN}Successed${NC}" || echo "${RED}Failed${NC}"

printf "DMPreview_TI Build : "
[ -f "${QT_DIR}/DMPreview_TI" ] && echo "${GREEN}Successed${NC}" || echo "${RED}Failed${NC}"
echo "========================================"