import unreal
import socket
import threading
import json
import traceback
import queue

# Configuration
HOST = '127.0.0.1'
PORT = 9999
BUFFER_SIZE = 1024 * 64  # 64KB

# Global State
main_thread_queue = queue.Queue()
server_running = True

def log(message):
    unreal.log(f"[PythonBridge] {message}")

def log_error(message):
    unreal.log_error(f"[PythonBridge] {message}")

def execution_loop(delta_seconds):
    """
    Called every tick on the Game Thread.
    Processes requests from the queue.
    """
    while not main_thread_queue.empty():
        client_socket, code, command_type = main_thread_queue.get()
        
        response = {
            "status": "error", 
            "output": "", 
            "error": ""
        }

        try:
            # Capture stdout
            import io
            from contextlib import redirect_stdout
            
            output_buffer = io.StringIO()
            
            if command_type == "evaluate":
                # Evaluate expression
                result = eval(code, globals())
                response["output"] = str(result)
                response["status"] = "ok"
            else:
                # Execute code block
                with redirect_stdout(output_buffer):
                    exec(code, globals())
                response["output"] = output_buffer.getvalue()
                response["status"] = "ok"
                
        except Exception as e:
            response["error"] = f"{str(e)}\n{traceback.format_exc()}"
            log_error(f"Execution Error: {response['error']}")
        
        # Send response back to client
        try:
            response_data = json.dumps(response).encode('utf-8')
            client_socket.sendall(response_data)
        except Exception as e:
            log_error(f"Failed to send response: {e}")
        finally:
            client_socket.close()

def server_thread_func():
    """
    Runs in a background thread to accept TCP connections.
    """
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        server.bind((HOST, PORT))
        server.listen(5)
        log(f"Server listening on {HOST}:{PORT}")
        
        while server_running:
            try:
                client, addr = server.accept()
                # log(f"Connection from {addr}")
                
                # Receive data
                data = b""
                while True:
                    packet = client.recv(BUFFER_SIZE)
                    data += packet
                    if len(packet) < BUFFER_SIZE:
                        break
                
                if not data:
                    client.close()
                    continue
                
                # Parse Request
                try:
                    msg = json.loads(data.decode('utf-8'))
                    code = msg.get("code", "")
                    command_type = msg.get("command", "execute") # execute or evaluate
                    
                    if code:
                        main_thread_queue.put((client, code, command_type))
                    else:
                        client.close()
                        
                except Exception as e:
                    log_error(f"Invalid request: {e}")
                    client.close()
                    
            except OSError:
                break
                
    except Exception as e:
        log_error(f"Server crashed: {e}")
    finally:
        server.close()

# Start Server
log("Initializing Python Bridge...")
t = threading.Thread(target=server_thread_func, daemon=True)
t.start()

# Register Tick Callback
handler = unreal.register_slate_post_tick_callback(execution_loop)
log("Registered PostTick callback.")
