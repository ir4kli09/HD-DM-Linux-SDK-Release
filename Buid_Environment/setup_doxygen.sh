#!/bin/sh

echo "1. Install doxygen for auto generate release document."
echo "======================================================"
sudo apt-get install doxygen
echo "======================================================"

echo "2. Install LaTex for transform latex to PDF."
echo "======================================================"
sudo apt-get install texlive-base texlive texlive-latex-recommended texlive-latex-extra
echo "======================================================"

