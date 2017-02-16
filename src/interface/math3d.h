#ifndef Z_GAME_MATH3D
#define Z_GAME_MATH3D

typedef struct Matrix_4x4
{
	float data[16];

} Matrix_4x4;

typedef struct Vector_3
{
	float x;
	float y;
	float z;

} Vector_3;

Matrix_4x4 get_perspective_projection_matrix(float vertical_fov, float aspect, float z_near, float z_far);
Matrix_4x4 get_view_matrix(Vector_3 eye, Vector_3 look_at, Vector_3 up);

Vector_3 get_subtracted_vector(const Vector_3 v0, const Vector_3 v1);
Vector_3 get_normalized_vector(const Vector_3 original_vector);
Vector_3 get_cross_product(const Vector_3 v0, const Vector_3 v1);

float get_dot_product(const Vector_3 v0, const Vector_3 v1);

#endif