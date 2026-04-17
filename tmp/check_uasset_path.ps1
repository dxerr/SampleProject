$filePath = "c:\wz\ExFrameWork\Plugins\GameFeatures\ExRunnerPlay\Content\Movies\kling_20260406_VIDEO____1_______3602_0.uasset"
$bytes = [System.IO.File]::ReadAllBytes($filePath)
$text = [System.Text.Encoding]::ASCII.GetString($bytes)
if ($text -match "([A-Z]:/[^`0]+)") {
    Write-Host "Found Absolute Path: $($matches[1])"
} else {
    Write-Host "No Absolute Path found."
}

$filePathTex = "c:\wz\ExFrameWork\Plugins\GameFeatures\ExRunnerPlay\Content\Movies\kling_20260406_VIDEO____1_______3602_0_Tex.uasset"
$bytesTex = [System.IO.File]::ReadAllBytes($filePathTex)
$textTex = [System.Text.Encoding]::ASCII.GetString($bytesTex)
if ($textTex -match "([A-Z]:/[^`0]+)") {
    Write-Host "Found Absolute Path in Tex: $($matches[1])"
} else {
    Write-Host "No Absolute Path found in Tex."
}
