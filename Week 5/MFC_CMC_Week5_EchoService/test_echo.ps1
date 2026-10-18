# Client test cho EchoService: gui tung dong go vao, in ra du lieu server tra ve.
# Chay: powershell -ExecutionPolicy Bypass -File test_echo.ps1 [-Server 127.0.0.1] [-Port 9999]
# Go dong trong (Enter) de thoat.
param([string]$Server = "127.0.0.1", [int]$Port = 9999)

$utf8 = New-Object System.Text.UTF8Encoding($false)
$client = New-Object System.Net.Sockets.TcpClient($Server, $Port)
$stream = $client.GetStream()
$stream.ReadTimeout = 3000
Write-Host "Da ket noi $Server`:$Port"

while ($true) {
    $line = Read-Host "Gui"
    if ([string]::IsNullOrEmpty($line)) { break }

    $out = $utf8.GetBytes($line)
    $stream.Write($out, 0, $out.Length)

    # Doc cho toi khi nhan du so byte da gui (TCP co the tra ve thanh nhieu goi)
    $in = New-Object byte[] $out.Length
    $got = 0
    while ($got -lt $out.Length) {
        $n = $stream.Read($in, $got, $out.Length - $got)
        if ($n -le 0) { throw "Server dong ket noi" }
        $got += $n
    }
    Write-Host ("Echo: " + $utf8.GetString($in, 0, $got))
}

$client.Close()
