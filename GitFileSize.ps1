git diff --cached --name-only | ForEach-Object {
    $item = Get-Item $_
    [PSCustomObject]@{
        'Size (KB)' = [math]::Round($item.Length / 1KB, 0)
        'Formatted Size (KB)' = "{0:N0} KB" -f ($item.Length / 1KB)
        'File Name' = $item.Name
        'Path' = $item.FullName.Substring((Get-Location).Path.Length + 1)
    }
} | Sort-Object -Property 'Size (KB)' | Select-Object 'Formatted Size (KB)', 'File Name', 'Path' | Format-Table -AutoSize
