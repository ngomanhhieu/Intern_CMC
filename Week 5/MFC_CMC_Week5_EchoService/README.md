# Tuần 5: Registry, Autorun, Windows Service

## 1. MFC_CMC_Week5_Autorun: đăng ký autorun cho app Chat

Đây là app chat TCP của tuần 4, có thêm checkbox **"Tự khởi động cùng Windows"** ở màn hình đăng nhập.

- Khi tick, app ghi vào registry
  `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, value `CMC_Chat` = `"<đường dẫn exe>"`.
- Khi bỏ tick, app xoá value đó đi.
- Lúc mở app, checkbox tự đọc registry để hiện đúng trạng thái.
- Dùng HKCU nên không cần quyền admin. Mỗi lần user đăng nhập Windows, app sẽ tự chạy.
- Cách kiểm tra: mở `regedit`, vào key ở trên, hoặc xem tab **Startup** trong Task Manager.

## 2. MFC_CMC_Week5_EchoService: Windows service echo TCP cổng 9999

Service mở cổng TCP 9999 và gửi trả lại cho client mọi byte mà client gửi lên. Mỗi client chạy trên một thread riêng.

Mở cmd **bằng quyền Administrator** rồi chạy:

```
EchoService.exe install       :: đăng ký service (tên: CMCEchoService)
sc start CMCEchoService       :: chạy service
sc query CMCEchoService       :: xem trạng thái
sc stop  CMCEchoService       :: dừng service
EchoService.exe uninstall     :: dừng (nếu đang chạy) rồi xoá service
EchoService.exe console       :: chạy như app console để debug, Ctrl+C để dừng
```

Test bằng client: `powershell -ExecutionPolicy Bypass -File test_echo.ps1`
(nếu chạy ở máy khác thì thêm `-Server <IP>` và mở firewall cho cổng 9999).

Log được ghi ra file `EchoService.log`, nằm cạnh file exe.

### Dừng service mà không bị treo
- `CtrlHandler` chỉ báo `SERVICE_STOP_PENDING` và bật `g_stopEvent`, rồi trả về ngay. Việc dọn dẹp được làm ở `ServiceMain`.
- Server đóng socket lắng nghe, làm `accept()` trả về, nên thread accept thoát.
- Mỗi client thread dùng `select` với timeout 200ms để kiểm tra cờ stop. Lý do là trên Windows, gọi `shutdown()` từ thread khác không làm `recv()` đang block trả về.
- `send()` có `SO_SNDTIMEO` là 2 giây. Nhờ vậy, client không chịu đọc dữ liệu cũng không làm thread kẹt mãi.
- Server chờ các client thread thoát, nhưng tối đa 5 giây. Sau đó nó báo `SERVICE_STOPPED`.
