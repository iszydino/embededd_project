# MCU Project: He thong can va phan loai san pham (STM32F103 + FreeRTOS)

## Nhanh
- main: ban on dinh, cam commit truc tiep
- develop: nhanh tich hop chinh
- feature/<ten-task>: nhanh lam viec ca nhan
- integration/<phien-ban>: ghep noi he thong truoc khi release

## Cau truc
- bsp/ : board support, pin mapping
- hal/ : driver ngoai vi (inc/, src/)
- middlewares/ : FreeRTOS (Kernel, Port, Heap)
- app/ : FreeRTOS tasks va IPC (inc/, src/)

## Quy tac commit
feat(task-xxx) | fix(rtos) | refactor(...)
