using CitizenFX.Core;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CustomCameraVScript
{
    class Extensions
    {
        public static Vector3 WorldUp
        {
            get
            {
                return new Vector3(0.0f, 0.0f, 1.0f);
            }
        }

        public static Vector3 RelativeFront
        {
            get
            {
                return new Vector3(0.0f, 1.0f, 0.0f);
            }
        }
        
        public static Vector3 RelativeBack
        {
            get
            {
                return new Vector3(0.0f, -1.0f, 0.0f);
            }
        }

        public static Vector3 RelativeBottom
        {
            get
            {
                return new Vector3(0.0f, 0.0f, -0.0f);
            }
        }

        public static Quaternion Euler(Vector3 vector)
        {
            Vector3 eulerRad = vector * ((float)Math.PI / 180.0f);
            return Quaternion.RotationYawPitchRoll(eulerRad.X, eulerRad.Y, eulerRad.Z);
        }
    }
}
