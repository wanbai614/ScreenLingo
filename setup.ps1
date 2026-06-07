# ScreenLingo — one-time setup script
# Downloads ONNX Runtime and PaddleOCR models for offline OCR support.
# Windows OCR and Python-subprocess OCR engines work without this step.

param(
    [switch]$SkipModels  # skip large model downloads (~300MB)
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "=== ScreenLingo Setup ===" -ForegroundColor Cyan

# --- ONNX Runtime (needed for PaddleOCR C++ engine) ---
$OrtDir = "$Root\lib\onnxruntime"
if (-not (Test-Path "$OrtDir\include\onnxruntime_c_api.h")) {
    Write-Host "[1/2] Downloading ONNX Runtime 1.21.0..." -ForegroundColor Yellow
    $OrtUrl = "https://github.com/microsoft/onnxruntime/releases/download/v1.21.0/onnxruntime-win-x64-1.21.0.zip"
    $OrtZip = "$env:TEMP\ort.zip"
    Invoke-WebRequest -Uri $OrtUrl -OutFile $OrtZip
    Expand-Archive -Path $OrtZip -DestinationPath "$Root\lib" -Force
    # Rename extracted folder to onnxruntime
    $extracted = Get-ChildItem "$Root\lib" -Directory | Where-Object { $_.Name -like "onnxruntime-win-x64-*" } | Select-Object -First 1
    if ($extracted -and $extracted.Name -ne "onnxruntime") {
        Rename-Item $extracted.FullName "$Root\lib\onnxruntime"
    }
    Remove-Item $OrtZip
    Write-Host "  ONNX Runtime installed to lib/onnxruntime/" -ForegroundColor Green
} else {
    Write-Host "[1/2] ONNX Runtime already present" -ForegroundColor Green
}

# --- PaddleOCR Models (PP-OCRv4) ---
if (-not $SkipModels) {
    $ModelDir = "$Root\package\models"
    if (-not (Test-Path "$ModelDir\ch_PP-OCRv4_det_infer.onnx")) {
        Write-Host "[2/2] Downloading PP-OCRv4 models (~300MB)..." -ForegroundColor Yellow
        New-Item -ItemType Directory -Force -Path $ModelDir | Out-Null

        $DetUrl = "https://www.modelscope.cn/api/v1/models/RapidAI/RapidOCR/repo?Revision=master&FilePath=ch_PP-OCRv4_det_infer.onnx"
        $RecUrl = "https://www.modelscope.cn/api/v1/models/RapidAI/RapidOCR/repo?Revision=master&FilePath=ch_PP-OCRv4_rec_infer.onnx"
        $KeysUrl = "https://paddleocr.bj.bcebos.com/ppocr_keys_v1.txt"

        Invoke-WebRequest -Uri $DetUrl  -OutFile "$ModelDir\ch_PP-OCRv4_det_infer.onnx"
        Invoke-WebRequest -Uri $RecUrl  -OutFile "$ModelDir\ch_PP-OCRv4_rec_infer.onnx"
        Invoke-WebRequest -Uri $KeysUrl -OutFile "$ModelDir\ppocr_keys_v1.txt"
        Write-Host "  PP-OCRv4 models installed to package/models/" -ForegroundColor Green
    } else {
        Write-Host "[2/2] PP-OCRv4 models already present" -ForegroundColor Green
    }
}

Write-Host "=== Setup complete ===" -ForegroundColor Cyan
Write-Host "Build: cmake -B build -G 'Visual Studio 17 2022' -A x64 -DCMAKE_PREFIX_PATH=D:/Qt/6.9.1/msvc2022_64"
Write-Host "Then:  cmake --build build --config Release"
