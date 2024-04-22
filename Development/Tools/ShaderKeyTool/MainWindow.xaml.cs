using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Collections.ObjectModel;


namespace EpicGames.Tools.ShaderKeyTool
{
    public class ShaderKey
    {
        public int Offset { get; set; }
        public int Bits { get; set; }
        public string Key { get; set; }
        public string Value { get; set; }
        public bool NonDefault { get; set; }
    }


    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        #region Fields

        /// <summary>
        /// Holds the current shader key values.
        /// </summary>

        ObservableCollection<ShaderKey> shaderKeyCollection = new ObservableCollection<ShaderKey>();

        /// <summary>
        /// Holds the current bit offset within the key part being parsed.
        /// </summary>

        protected int offset;

        #endregion


        #region Properties

        /// <summary>
        /// Gets the current shader key values.
        /// </summary>

        public ObservableCollection<ShaderKey> ShaderKeyCollection
        {
            get
            {
                return shaderKeyCollection;
            }
        }

        #endregion


        #region Constructors

        /// <summary>
        /// Creates and initializes a new instance of the main window.
        /// </summary>

        public MainWindow ()
        {
            InitializeComponent();

            textBoxInput.Focus();
        }

        #endregion


        #region Implementation

        /// <summary>
        /// Attempts to parse the shader key string in the input text box.
        /// </summary>

        protected void ParseInput ()
        {
            shaderKeyCollection.Clear();

            string[] keys = textBoxInput.Text.Trim().Split('_');

            UInt64 keyLow = 0;
            UInt64 keyHigh = 0;

            try
            {
                keyLow = Convert.ToUInt64(keys[1].Substring(2), 16);
                keyHigh = Convert.ToUInt64(keys[0].Substring(2), 16);

                textBoxInput.Foreground = new SolidColorBrush(Colors.Black);
            }
            catch
            {
                textBoxInput.Foreground = new SolidColorBrush(Colors.Red);

                return;
            }

            offset = 0;

            // key 0
            ParseBits(ref keyLow, 2, "PKDT_AlphaValueSource", new string[] { "MAVS_DiffuseTextureAlpha", "MAVS_MaskTextureRed", "MAVS_MaskTextureGreen", "MAVS_MaskTextureBlue" });
            ParseBits(ref keyLow, 5, "PKDT_EmissiveMaskSource", new string[] { "MVS_Constant", "MVS_VertexColorRed", "MVS_VertexColorGreen", "MVS_VertexColorBlue", "MVS_VertexColorAlpha", "MVS_BaseTextureRed", "MVS_BaseTextureGreen", "MVS_BaseTextureBlue", "MVS_BaseTextureAlpha", "MVS_MaskTextureRed", "MVS_MaskTextureGreen", "MVS_MaskTextureBlue", "MVS_MaskTextureAlpha", "MVS_NormalTextureAlpha", "MVS_EmissiveTextureRed", "MVS_EmissiveTextureGreen", "MVS_EmissiveTextureBlue", "MVS_EmissiveTextureAlpha" });
            ParseBits(ref keyLow, 2, "PKDT_EmissiveColorSource", new string[] { "MECS_EmissiveTexture", "MECS_BaseTexture", "MECS_Constant" });
            ParseBits(ref keyLow, 1, "PKDT_IsEmissiveEnabled");
            ParseBits(ref keyLow, 5, "PKDT_EnvironmentMaskSource", new string[] { "MVS_Constant", "MVS_VertexColorRed", "MVS_VertexColorGreen", "MVS_VertexColorBlue", "MVS_VertexColorAlpha", "MVS_BaseTextureRed", "MVS_BaseTextureGreen", "MVS_BaseTextureBlue", "MVS_BaseTextureAlpha", "MVS_MaskTextureRed", "MVS_MaskTextureGreen", "MVS_MaskTextureBlue", "MVS_MaskTextureAlpha", "MVS_NormalTextureAlpha", "MVS_EmissiveTextureRed", "MVS_EmissiveTextureGreen", "MVS_EmissiveTextureBlue", "MVS_EmissiveTextureAlpha" });
            ParseBits(ref keyLow, 5, "PKDT_RimLightingMaskSource", new string[] { "MVS_Constant", "MVS_VertexColorRed", "MVS_VertexColorGreen", "MVS_VertexColorBlue", "MVS_VertexColorAlpha", "MVS_BaseTextureRed", "MVS_BaseTextureGreen", "MVS_BaseTextureBlue", "MVS_BaseTextureAlpha", "MVS_MaskTextureRed", "MVS_MaskTextureGreen", "MVS_MaskTextureBlue", "MVS_MaskTextureAlpha", "MVS_NormalTextureAlpha", "MVS_EmissiveTextureRed", "MVS_EmissiveTextureGreen", "MVS_EmissiveTextureBlue", "MVS_EmissiveTextureAlpha" });

            ParseBits(ref keyLow, 1, "PKDT_IsRimLightingEnabled");
            ParseBits(ref keyLow, 1, "PKDT_UseVertexColorMultiply");
            ParseBits(ref keyLow, 1, "PKDT_UseUniformColorMultiply");
            ParseBits(ref keyLow, 3, "PKDT_AmbientOcclusionSource", new string[] { "MAOS_Disabled", "MAOS_VertexColorRed", "MAOS_VertexColorGreen", "MAOS_VertexColorBlue", "MAOS_VertexColorAlpha" });
            ParseBits(ref keyLow, 4, "PKDT_SpecularMask", new string[] { "MSM_Constant", "MSM_Luminance", "MSM_DiffuseRed", "MSM_DiffuseGreen", "MSM_DiffuseBlue", "MSM_DiffuseAlpha", "MSM_MaskTextureRGB", "MSM_MaskTextureRed", "MSM_MaskTextureGreen", "MSM_MaskTextureBlue", "MSM_MaskTextureAlpha" });
            ParseBits(ref keyLow, 1, "PKDT_TextureBlendFactorSource");
            ParseBits(ref keyLow, 1, "PKDT_IsUsingOneDetailTexture");

            ParseBits(ref keyLow, 1, "PKDT_IsUsingTwoDetailTexture");
            ParseBits(ref keyLow, 1, "PKDT_IsUsingThreeDetailTexture");
            ParseBits(ref keyLow, 1, "PKDT_MobileEnvironmentBlendMode");
            ParseBits(ref keyLow, 1, "PKDT_IsEnvironmentMappingEnabled");

            ParseBits(ref keyLow, 1, "PKDT_IsDetailTextureTransformed");
            ParseBits(ref keyLow, 1, "PKDT_IsMaskTextureTransformed");
            ParseBits(ref keyLow, 1, "PKDT_IsNormalTextureTransformed");
            ParseBits(ref keyLow, 1, "PKDT_IsEmissiveTextureTransformed");

            ParseBits(ref keyLow, 1, "PKDT_IsBaseTextureTransformed");

            ParseBits(ref keyLow, 2, "PKDT_MaskTextureTexCoordsSource", new string[] { "MTCS_TexCoords0", "MTCS_TexCoords1", "MTCS_TexCoords2", "MTCS_TexCoords3" });
            ParseBits(ref keyLow, 2, "PKDT_DetailTextureTexCoordsSource", new string[] { "MTCS_TexCoords0", "MTCS_TexCoords1", "MTCS_TexCoords2", "MTCS_TexCoords3" });
            ParseBits(ref keyLow, 2, "PKDT_BaseTextureTexCoordsSource", new string[] { "MTCS_TexCoords0", "MTCS_TexCoords1", "MTCS_TexCoords2", "MTCS_TexCoords3" });
            ParseBits(ref keyLow, 3, "PKDT_BlendMode", new string[] { "BLEND_Opaque", "BLEND_Masked", "BLEND_Translucent", "BLEND_Additive", "BLEND_Modulate", "BLEND_ModulateAndAdd", "BLEND_SoftMasked", "BLEND_AlphaComposite", "BLEND_DitheredTranslucent" });

            ParseBits(ref keyLow, 1, "PKDT_IsSubUV");
            ParseBits(ref keyLow, 1, "PKDT_IsDecal");
            ParseBits(ref keyLow, 1, "PKDT_IsSkinned");
            ParseBits(ref keyLow, 1, "PKDT_IsLightmap");

            ParseBits(ref keyLow, 1, "PKDT_UseGammaCorrection");
            ParseBits(ref keyLow, 2, "PKDT_ParticleScreenAlignment");
            ParseBits(ref keyLow, 1, "PKDT_IsGradientFogEnabled");
            ParseBits(ref keyLow, 1, "PKDT_IsDepthOnlyRendering");

            ParseBits(ref keyLow, 3, "PKDT_PrimitiveType", new string[] { "EPT_Default", "EPT_Particle", "EPT_BeamTrailParticle", "EPT_LensFlare", "EPT_Simple", "EPT_DistanceFieldFont", "EPT_GlobalShader" });
            ParseBits(ref keyLow, 2, "PKDT_PlatformFeatures", new string[] { "EPF_HighEndFeatures", "EPF_LowEndFeatures"});

            offset = 0;

            // key 1
            ParseBits(ref keyHigh, 1, "PKDT_IsGfxGammaCorrectionEnabled");
            ParseBits(ref keyHigh, 1, "PKDT_UseLandscapeMonochromeLayerBlending");
            ParseBits(ref keyHigh, 1, "PKDT_IsLandscape");
            ParseBits(ref keyHigh, 1, "PKDT_UseFallbackStreamColor");
            ParseBits(ref keyHigh, 4, "PKDT_ColorMultiplySource", new string[] { "MCMS_None", "MCMS_BaseTextureRed", "MCMS_BaseTextureGreen", "MCMS_BaseTextureBlue", "MCMS_BaseTextureAlpha", "MCMS_MaskTextureRed", "MCMS_MaskTextureGreen", "MCMS_MaskTextureBlue", "MCMS_MaskTextureAlpha" });
            ParseBits(ref keyHigh, 5, "PKDT_GfxBlendMode", new string[] { "EGFXBM_Disabled", "EGFXBM_Normal", "EGFXBM_Add", "EGFXBM_Subtract", "EGFXBM_Multiply", "EGFXBM_Darken", "EGFXBM_Lighten", "EGFXBM_None", "EGFXBM_SourceAc" });
            ParseBits(ref keyHigh, 1, "PKDT_IsMobileColorGradingEnabled");
            ParseBits(ref keyHigh, 1, "PKDT_IsBumpOffsetEnabled");
            ParseBits(ref keyHigh, 1, "PKDT_IsMobileEnvironmentFresnelEnabled");
            ParseBits(ref keyHigh, 1, "PKDT_IsDetailNormalEnabled");
            ParseBits(ref keyHigh, 1, "PKDT_IsWaveVertexMovementEnabled");

            ParseBits(ref keyHigh, 1, "PKDT_TwoSided");
            ParseBits(ref keyHigh, 1, "PKDT_IsHeightFogEnabled");
            ParseBits(ref keyHigh, 1, "PKDT_IsNormalMappingEnabled");
            ParseBits(ref keyHigh, 1, "PKDT_IsPixelSpecularEnabled");

            ParseBits(ref keyHigh, 1, "PKDT_IsSpecularEnabled");
            ParseBits(ref keyHigh, 1, "PKDT_IsLightingEnabled");
            ParseBits(ref keyHigh, 1, "PKDT_IsDirectionalLightmap");
            ParseBits(ref keyHigh, 1, "PKDT_ForwardShadowProjectionShaderType");

            ParseBits(ref keyHigh, 2, "PKDT_DepthShaderType");
            ParseBits(ref keyHigh, 0, "PKDT_GlobalShaderType2");
            ParseBits(ref keyHigh, 10, "PKDT_GlobalShaderType", new string[] { "EGST_None", "EGST_GammaCorrection", "EGST_Filter1", "EGST_Filter4", "EGST_Filter16", "EGST_DOFAndBloomGather", "EGST_LUTBlender", "EGST_UberPostProcess", "EGST_LightShaftDownSample", "EGST_LightShaftDownSample_NoDepth", "EGST_LightShaftBlur", "EGST_LightShaftApply", "EGST_SimpleF32", "EGST_PositionOnly", "EGST_ShadowProjection", "EGST_DOFGather", "EGST_MobileUberPostProcess", "EGST_MobileUberPostProcessNoColorGrading", "EGST_VisualizeTexture" });
        }

        /// <summary>
        /// Parses the specified number of bits in the given bits.
        /// </summary>
        /// <param name="bits">The remaining bits to parse.</param>
        /// <param name="numBits">The number of bits to parse.</param>
        /// <param name="name">The name of the key to parse.</param>
        /// <param name="enums">Optional list of names for enum values.</param>

        protected void ParseBits (ref UInt64 bits, int numBits, string name, string[] enums = null)
        {
            if (numBits > 0)
            {
                UInt64 mask = (UInt64)(Math.Pow(2, numBits) - 1);
                int result = (int)(bits & mask);

                shaderKeyCollection.Add(new ShaderKey
                {
                    Offset = offset,
                    Bits = numBits,
                    Key = name,
                    Value = ((enums != null) && (enums.Length > result)) ? enums[result] : result.ToString(),
                    NonDefault = (result != 0)
                });

                bits >>= numBits;
                offset += numBits;
            }
        }

        #endregion


        #region Event Handlers

        private void buttonParse_Click (object sender, RoutedEventArgs e)
        {
            ParseInput();
        }


        private void textboxInput_KeyDown (object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Return)
            {
                ParseInput();
            }
        }


        private void textBoxInput_TextChanged (object sender, RoutedEventArgs e)
        {
            ParseInput();
        }

        #endregion
    }
}