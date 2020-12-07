int   getDiffFactor(const unsigned char* color1, const unsigned char* color2, const  int & channels)
{
    int final_diff;
    int component_diff[4];

    // find absolute difference between each component
    for (int i = 0; i < channels; i++)
    {
        component_diff[i] = abs(color1[i] - color2[i]);
    }

    // based on number of components, produce a single difference value in the 0-255 range
    switch (channels)
    {
    case 1:
        final_diff = component_diff[0];
        break;

    case 2:
        final_diff = ((component_diff[0] + component_diff[1]) >> 1);
        break;

    case 3:
        final_diff = ((component_diff[0] + component_diff[2]) >> 2) + (component_diff[1] >> 1);
        break;

    case 4:
        final_diff = ((component_diff[0] + component_diff[1] + component_diff[2] + component_diff[3]) >> 2);
        break;

    default:
        final_diff = 0;
    }

    _ASSERT(final_diff >= 0 && final_diff <= 255);

    return final_diff;
}

void CRB_HorizontalFilter(unsigned char* Input, unsigned char* Output, int Width, int Height, int Channels, float * range_table_f, float inv_alpha_f, float* left_Color_Buffer, float* left_Factor_Buffer, float* right_Color_Buffer, float* right_Factor_Buffer)
{

    // Left pass and Right pass 

    int Stride = Width * Channels;
    const unsigned char* src_left_color = Input;
    float* left_Color = left_Color_Buffer;
    float* left_Factor = left_Factor_Buffer;

    int last_index = Stride * Height - 1;
    const unsigned char* src_right_color = Input + last_index;
    float* right_Color = right_Color_Buffer + last_index;
    float* right_Factor = right_Factor_Buffer + Width * Height - 1;

    for (int y = 0; y < Height; y++)
    {
        const unsigned char* src_left_prev = Input;
        const float* left_prev_factor = left_Factor;
        const float* left_prev_color = left_Color;

        const unsigned char* src_right_prev = src_right_color;
        const float* right_prev_factor = right_Factor;
        const float* right_prev_color = right_Color;

        // process 1st pixel separately since it has no previous
        {
            //if x = 0 
            *left_Factor++ = 1.f;
            *right_Factor-- = 1.f;
            for (int c = 0; c < Channels; c++)
            {
                *left_Color++ = *src_left_color++;
                *right_Color-- = *src_right_color--;
            }
        }
        // handle other pixels
        for (int x = 1; x < Width; x++)
        {
            // determine difference in pixel color between current and previous
            // calculation is different depending on number of channels
            int left_diff = getDiffFactor(src_left_color, src_left_prev, Channels);
            src_left_prev = src_left_color;

            int right_diff = getDiffFactor(src_right_color, src_right_color - Channels, Channels);
            src_right_prev = src_right_color;

            float left_alpha_f = range_table_f[left_diff];
            float right_alpha_f = range_table_f[right_diff];
            *left_Factor++ = inv_alpha_f + left_alpha_f * (*left_prev_factor++);
            *right_Factor-- = inv_alpha_f + right_alpha_f * (*right_prev_factor--);

            for (int c = 0; c < Channels; c++)
            {
                *left_Color++ = (inv_alpha_f * (*src_left_color++) + left_alpha_f * (*left_prev_color++));
                *right_Color-- = (inv_alpha_f * (*src_right_color--) + right_alpha_f * (*right_prev_color--));
            }
        }
    }
    // vertical pass will be applied on top on horizontal pass, while using pixel differences from original image
    // result color stored in 'leftcolor' and vertical pass will use it as source color
    {
        unsigned char* dst_color = Output; // use as temporary buffer  
        const float* leftcolor = left_Color_Buffer;
        const float* leftfactor = left_Factor_Buffer;
        const float* rightcolor = right_Color_Buffer;
        const float* rightfactor = right_Factor_Buffer;

        int width_height = Width * Height;
        for (int i = 0; i < width_height; i++)
        {
            // average color divided by average factor
            float factor = 1.f / ((*leftfactor++) + (*rightfactor++));
            for (int c = 0; c < Channels; c++)
            {

                *dst_color++ = (factor * ((*leftcolor++) + (*rightcolor++)));

            }
        }
    }
}

void CRB_VerticalFilter(unsigned char* Input, unsigned char* Output, int Width, int Height, int Channels, float * range_table_f, float inv_alpha_f, float* down_Color_Buffer, float* down_Factor_Buffer, float* up_Color_Buffer, float* up_Factor_Buffer)
{

    // Down pass and Up pass 
    int Stride = Width * Channels;
    const unsigned char* src_color_first_hor = Output; // result of horizontal pass filter 
    const unsigned char* src_down_color = Input;
    float* down_color = down_Color_Buffer;
    float* down_factor = down_Factor_Buffer;

    const unsigned char* src_down_prev = src_down_color;
    const float* down_prev_color = down_color;
    const float* down_prev_factor = down_factor;


    int last_index = Stride * Height - 1;
    const unsigned char* src_up_color = Input + last_index;
    const unsigned char* src_color_last_hor = Output + last_index; // result of horizontal pass filter
    float* up_color = up_Color_Buffer + last_index;
    float* up_factor = up_Factor_Buffer + (Width * Height - 1);

    const float* up_prev_color = up_color;
    const float* up_prev_factor = up_factor;

    // 1st line done separately because no previous line
    {
        //if y=0 
        for (int x = 0; x < Width; x++)
        {
            *down_factor++ = 1.f;
            *up_factor-- = 1.f;
            for (int c = 0; c < Channels; c++)
            {
                *down_color++ = *src_color_first_hor++;
                *up_color-- = *src_color_last_hor--;
            }
            src_down_color += Channels;
            src_up_color -= Channels;
        }
    }
    // handle other lines 
    for (int y = 1; y < Height; y++)
    {
        for (int x = 0; x < Width; x++)
        {
            // determine difference in pixel color between current and previous
            // calculation is different depending on number of channels
            int down_diff = getDiffFactor(src_down_color, src_down_prev, Channels);
            src_down_prev += Channels;
            src_down_color += Channels;
            src_up_color -= Channels;
            int up_diff = getDiffFactor(src_up_color, src_up_color + Stride, Channels);
            float down_alpha_f = range_table_f[down_diff];
            float up_alpha_f = range_table_f[up_diff];

            *down_factor++ = inv_alpha_f + down_alpha_f * (*down_prev_factor++);
            *up_factor-- = inv_alpha_f + up_alpha_f * (*up_prev_factor--);

            for (int c = 0; c < Channels; c++)
            {
                *down_color++ = inv_alpha_f * (*src_color_first_hor++) + down_alpha_f * (*down_prev_color++);
                *up_color-- = inv_alpha_f * (*src_color_last_hor--) + up_alpha_f * (*up_prev_color--);
            }
        }
    }

    // average result of vertical pass is written to output buffer
    {
        unsigned char *dst_color = Output;
        const float* downcolor = down_Color_Buffer;
        const float* downfactor = down_Factor_Buffer;
        const float* upcolor = up_Color_Buffer;
        const float* upfactor = up_Factor_Buffer;

        int width_height = Width * Height;
        for (int i = 0; i < width_height; i++)
        {
            // average color divided by average factor
            float factor = 1.f / ((*upfactor++) + (*downfactor++));
            for (int c = 0; c < Channels; c++)
            {
                *dst_color++ = (factor * ((*upcolor++) + (*downcolor++)));
            }
        }
    }
}

// memory must be reserved before calling image filter
// this implementation of filter uses plain C++, single threaded
// channel count must be 3 or 4 (alpha not used)
void    CRBFilter(unsigned char* Input, unsigned char* Output, int Width, int Height, int Stride, float sigmaSpatial, float sigmaRange)
{
    int Channels = Stride / Width;
    int reserveWidth = Width;
    int reserveHeight = Height;
    // basic sanity check
    _ASSERT(Input);
    _ASSERT(Output);
    _ASSERT(reserveWidth >= 10 && reserveWidth < 10000);
    _ASSERT(reserveHeight >= 10 && reserveHeight < 10000);
    _ASSERT(Channels >= 1 && Channels <= 4);

    int reservePixels = reserveWidth * reserveHeight;
    int numberOfPixels = reservePixels * Channels;

    float* leftColorBuffer = (float*)calloc(sizeof(float)*numberOfPixels, 1);
    float* leftFactorBuffer = (float*)calloc(sizeof(float)*reservePixels, 1);
    float* rightColorBuffer = (float*)calloc(sizeof(float)*numberOfPixels, 1);
    float* rightFactorBuffer = (float*)calloc(sizeof(float)*reservePixels, 1);

    if (leftColorBuffer == NULL || leftFactorBuffer == NULL || rightColorBuffer == NULL || rightFactorBuffer == NULL)
    {
        if (leftColorBuffer)  free(leftColorBuffer);
        if (leftFactorBuffer) free(leftFactorBuffer);
        if (rightColorBuffer) free(rightColorBuffer);
        if (rightFactorBuffer) free(rightFactorBuffer);

        return;
    }
    float* downColorBuffer = leftColorBuffer;
    float* downFactorBuffer = leftFactorBuffer;
    float* upColorBuffer = rightColorBuffer;
    float* upFactorBuffer = rightFactorBuffer;
    // compute a lookup table
    float alpha_f = static_cast<float>(exp(-sqrt(2.0) / (sigmaSpatial * 255)));
    float inv_alpha_f = 1.f - alpha_f;


    float range_table_f[255 + 1];
    float inv_sigma_range = 1.0f / (sigmaRange * 255);

    float ii = 0.f;
    for (int i = 0; i <= 255; i++, ii -= 1.f)
    {
        range_table_f[i] = alpha_f * exp(ii * inv_sigma_range);
    }
    CRB_HorizontalFilter(Input, Output, Width, Height, Channels, range_table_f, inv_alpha_f, leftColorBuffer, leftFactorBuffer, rightColorBuffer, rightFactorBuffer);

    CRB_VerticalFilter(Input, Output, Width, Height, Channels, range_table_f, inv_alpha_f, downColorBuffer, downFactorBuffer, upColorBuffer, upFactorBuffer);

    if (leftColorBuffer)
    {
        free(leftColorBuffer);
        leftColorBuffer = NULL;
    }

    if (leftFactorBuffer)
    {
        free(leftFactorBuffer);
        leftFactorBuffer = NULL;
    }

    if (rightColorBuffer)
    {
        free(rightColorBuffer);
        rightColorBuffer = NULL;
    }

    if (rightFactorBuffer)
    {
        free(rightFactorBuffer);
        rightFactorBuffer = NULL;
    }
}
