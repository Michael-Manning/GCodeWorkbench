using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Net.Sockets;
using System.Threading;
using Renci.SshNet;
using System.Net;
using System.IO;
using System.Xml;
using System.Xml.Schema;
using System.Collections.Concurrent;

namespace SFTP_download
{
    class Program
    {
        /*
         * arguments:
         * 
         * method A
         * IP Address
         * Operation = download
         * filenameRead A
         * filenameWrite A
         * filenameRead B
         * filenameWriteB
         * ......
         * 
         * method B
         * IP Address
         * operation = scanSurface
         * xA
         * yA
         * xB
         * yB
         */

        static string Address;

        static int Main(string[] args)
        {
            //args = new string[] { "192.168.43.165", "download" , "/home/pi/imgs/pano1.jpg", "C:/Users/Micha/Documents/calibration pics/temp/pano1.jpg" };
            if (args.Length < 3)
            {
                Console.WriteLine("No enough arguments...");
                return 1;
            }

            Address = args[0];
            if(args[1].ToLower() == "download")
            {
                Console.WriteLine("Downloading...");
                for (int i = 2; i < args.Length; i+= 2)           
                    DownloadImage(args[i + 1], args[i]);
                Console.WriteLine("Finished");
                return 0;
            }
            //else if (args[1].ToLower() == "scansurface")
            //    ScanSurface(int.Parse(args[2]), int.Parse(args[3]), int.Parse(args[4]), int.Parse(args[5]));

            return 1;
        }


        /// <summary>
        /// Downloads a file from the PI Async
        /// </summary>
        /// <param name="saveLocation">Where to save the file locally</param>
        /// <param name="readLocation">Where on the pi to download from remotely eg. '/home/pi/download.jpeg'</param>
        public static async Task DownloadImageAsync(string saveLocation, string readLocation)
        {
            ConnectionInfo connection = new ConnectionInfo(Address, "pi", new PasswordAuthenticationMethod("pi", "attiny85"));
            using (var client = new SftpClient(connection))
            {
                using (FileStream file = File.Create(saveLocation))
                {
                    client.Connect();
                    await Task.Factory.StartNew(() => client.DownloadFile(readLocation, file));

                    file.Close();
                }
            }
        }
        /// <summary>
        /// Downloads a file from the PI
        /// </summary>
        /// <param name="saveLocation">Where to save the file locally</param>
        /// <param name="readLocation">Where on the pi to download from remotely eg. '/home/pi/download.jpeg'</param>
        public static void DownloadImage(string saveLocation, string readLocation)
        {
            try
            {
                ConnectionInfo connection = new ConnectionInfo(Address, "pi", new PasswordAuthenticationMethod("pi", "attiny85"));
                using (var client = new SftpClient(connection))
                {
                    using (FileStream file = File.Create(saveLocation))
                    {
                        client.Connect();
                        client.DownloadFile(readLocation, file);
                        file.Close();
                    }
                }
            }
            catch(Exception e)
            {
                Console.WriteLine(e.Message);
            }
        }
    }


    public class StitchImage
    {
        public string fileName;
        public float x, y;

        public StitchImage(string filepath, float x, float y)
        {
            this.fileName = filepath;
            this.x = x;
            this.y = y;
        }
    }
}
