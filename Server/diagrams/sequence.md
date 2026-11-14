```mermaid
sequenceDiagram
    participant S as TCP Echo Server (port 20095)
    participant T1 as Worker Thread 1
    participant T2 as Worker Thread 2
    participant C1 as Client #1
    participant C2 as Client #2

    Note over S: WSAStartup(), socket(), bind(), listen()
    
    C1->>S: connect(127.0.0.1:20095)
    S-->>C1: accept() -> —окет 1
    S->>T1: —оздание потока HandleClient(1)

    C2->>S: connect(127.0.0.1:20095)
    S-->>C2: accept() -> —окет 2
    S->>T2: —оздание потока HandleClient(2)

    C1->>T1: send(бинарный блок + нулевые байты)
    Note over T1: recv() -> буфер без изменений
    T1-->>C1: SendAll() -> Ёхо-ответ (RFC 862)
    Note over C1: RecvAll() и memcmp() == 0 (OK)

    C2->>T2: send(фрагментированные куски 333B)
    Note over T2: —клейка фрагментов в TCP-потоке
    T2-->>C2: SendAll() -> Ёхо-ответ
    Note over C2: RecvAll() и memcmp() == 0 (OK)

    Note over C1: ќкончание циклов
    C1->>T1: shutdown(SD_SEND) / FIN
    T1-->>C1: recv() == 0 -> closesocket()

    Note over C2: јварийный обрыв (Ctrl+C)
    C2--xT2: Reset (WSAECONNRESET)
    Note over T2: ќбработка ошибки 10054, поток завершаетс€, сервер продолжает работу