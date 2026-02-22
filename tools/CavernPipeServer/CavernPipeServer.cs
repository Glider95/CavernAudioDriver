using System;
using System.IO;
using System.IO.Pipes;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

namespace CavernPipeServer
{
    /// <summary>
    /// Cavern Pipe Server - Receives audio from kernel driver and forwards to Snapserver
    /// 
    /// Named Pipe: \\.\pipe\CavernAudioPipe
    /// Default Snapserver: localhost:1705
    /// </summary>
    class Program
    {
        private const string PIPE_NAME = "CavernAudioPipe";
        private const string SNAPSERVER_HOST = "localhost";
        private const int SNAPSERVER_PORT = 1705;
        private const int BUFFER_SIZE = 65536;
        
        private static bool _running = true;
        private static long _totalBytesReceived = 0;
        private static long _packetsReceived = 0;
        private static DateTime _startTime;
        
        static async Task Main(string[] args)
        {
            Console.WriteLine("╔══════════════════════════════════════════════════════════════╗");
            Console.WriteLine("║          Cavern Pipe Server v1.0 - Dolby Atmos Bridge        ║");
            Console.WriteLine("╚══════════════════════════════════════════════════════════════╝");
            Console.WriteLine();
            Console.WriteLine($"Named Pipe:  \\.\\pipe\\{PIPE_NAME}");
            Console.WriteLine($"Snapserver:  {SNAPSERVER_HOST}:{SNAPSERVER_PORT}");
            Console.WriteLine($"Buffer Size: {BUFFER_SIZE} bytes");
            Console.WriteLine();
            Console.WriteLine("Press Ctrl+C to exit");
            Console.WriteLine();
            
            _startTime = DateTime.Now;
            
            // Handle Ctrl+C
            Console.CancelKeyPress += (sender, e) =>
            {
                e.Cancel = true;
                _running = false;
                Console.WriteLine("\n[Shutdown requested...]");
            };
            
            // Start statistics thread
            _ = Task.Run(StatisticsThread);
            
            // Start pipe server
            await RunPipeServerAsync();
        }
        
        static async Task RunPipeServerAsync()
        {
            while (_running)
            {
                try
                {
                    await using var pipeServer = new NamedPipeServerStream(
                        PIPE_NAME,
                        PipeDirection.In,
                        1,  // Max 1 concurrent connection (from driver)
                        PipeTransmissionMode.Byte,
                        PipeOptions.Asynchronous,
                        BUFFER_SIZE,
                        0);
                    
                    Console.WriteLine("[Waiting for driver connection...]");
                    
                    await pipeServer.WaitForConnectionAsync();
                    Console.WriteLine("[Driver connected!]");
                    
                    // Handle client connection
                    await HandleClientAsync(pipeServer);
                }
                catch (IOException ex)
                {
                    Console.WriteLine($"[Pipe Error: {ex.Message}]");
                    await Task.Delay(1000);
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[Error: {ex.Message}]");
                    await Task.Delay(5000);
                }
            }
        }
        
        static async Task HandleClientAsync(NamedPipeServerStream pipeServer)
        {
            var buffer = new byte[BUFFER_SIZE];
            
            // Connect to snapserver
            TcpClient snapClient = null;
            NetworkStream snapStream = null;
            
            try
            {
                snapClient = new TcpClient();
                await snapClient.ConnectAsync(SNAPSERVER_HOST, SNAPSERVER_PORT);
                snapStream = snapClient.GetStream();
                Console.WriteLine("[Connected to Snapserver]");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[Snapserver connection failed: {ex.Message}]");
                Console.WriteLine("[Audio will be logged but not forwarded]");
            }
            
            // Also create a file for raw capture (for debugging)
            var captureFile = $"cavern_capture_{DateTime.Now:yyyyMMdd_HHmmss}.raw";
            await using var fileStream = new FileStream(captureFile, FileMode.Create, FileAccess.Write);
            Console.WriteLine($"[Capturing to: {captureFile}]");
            
            try
            {
                while (_running && pipeServer.IsConnected)
                {
                    int bytesRead = await pipeServer.ReadAsync(buffer, 0, buffer.Length);
                    
                    if (bytesRead == 0)
                    {
                        Console.WriteLine("[Driver disconnected]");
                        break;
                    }
                    
                    // Update statistics
                    _totalBytesReceived += bytesRead;
                    _packetsReceived++;
                    
                    // Write to capture file
                    await fileStream.WriteAsync(buffer, 0, bytesRead);
                    await fileStream.FlushAsync();
                    
                    // Forward to snapserver if connected
                    if (snapStream != null && snapClient?.Connected == true)
                    {
                        try
                        {
                            await snapStream.WriteAsync(buffer, 0, bytesRead);
                        }
                        catch
                        {
                            Console.WriteLine("[Snapserver connection lost]");
                            snapStream?.Dispose();
                            snapClient?.Dispose();
                            snapStream = null;
                            snapClient = null;
                        }
                    }
                    
                    // Detect format for logging (first packet only)
                    if (_packetsReceived == 1)
                    {
                        DetectAndLogFormat(buffer, bytesRead);
                    }
                }
            }
            catch (IOException)
            {
                Console.WriteLine("[Pipe disconnected]");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[Error handling client: {ex.Message}]");
            }
            finally
            {
                snapStream?.Dispose();
                snapClient?.Dispose();
                Console.WriteLine($"[Session ended - Captured: {_totalBytesReceived:N0} bytes]");
            }
        }
        
        static void DetectAndLogFormat(byte[] buffer, int length)
        {
            if (length < 4) return;
            
            // Check for AC3/E-AC3 (0x0B77)
            if (buffer[0] == 0x0B && buffer[1] == 0x77)
            {
                // Distinguish AC3 from E-AC3
                if (length >= 5)
                {
                    int strmtyp = (buffer[2] >> 5) & 0x3;
                    if (strmtyp == 0 || strmtyp == 2)
                    {
                        Console.WriteLine("[Format Detected: E-AC3 / Dolby Digital Plus]");
                        Console.WriteLine("[Atmos Support: YES]");
                        return;
                    }
                }
                Console.WriteLine("[Format Detected: AC3 / Dolby Digital]");
                return;
            }
            
            // Check for TrueHD (0xF8726FBA)
            uint syncWord = ((uint)buffer[0] << 24) | 
                            ((uint)buffer[1] << 16) | 
                            ((uint)buffer[2] << 8) | 
                            buffer[3];
            
            if (syncWord == 0xF8726FBA || syncWord == 0x72FBA870)
            {
                Console.WriteLine("[Format Detected: Dolby TrueHD]");
                Console.WriteLine("[Atmos Support: YES]");
                return;
            }
            
            // Check for DTS (0x7FFE8001)
            if (syncWord == 0x7FFE8001)
            {
                Console.WriteLine("[Format Detected: DTS]");
                return;
            }
            
            Console.WriteLine($"[Format Detected: Unknown (sync: 0x{syncWord:X8})]");
        }
        
        static async Task StatisticsThread()
        {
            while (_running)
            {
                await Task.Delay(TimeSpan.FromSeconds(5));
                
                if (!_running) break;
                
                var elapsed = DateTime.Now - _startTime;
                var rate = elapsed.TotalSeconds > 0 
                    ? _totalBytesReceived / elapsed.TotalSeconds 
                    : 0;
                
                Console.WriteLine($"[Stats] Packets: {_packetsReceived:N0} | Bytes: {_totalBytesReceived:N0} | Rate: {rate:N0} B/s");
            }
        }
    }
}
