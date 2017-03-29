#ifndef Z_GAME_MATH3D
#define Z_GAME_MATH3D

typedef struct Matrix4x4
{
	float data[16];

} Matrix4x4;
typedef struct Quaternion
{
	float x;
	float y;
	float z;
	float w;

} Quaternion;
typedef struct Vector3
{
	float x;
	float y;
	float z;

} Vector3;
typedef struct Vector4
{
	float x;
	float y;
	float z;
	float w;

} Vector4;

void update_perspective_projection_matrix(
	Matrix4x4 *proj,

	const float vertical_fov,
	const float aspect,
	const float z_near,
	const float z_far
);

Vector3 get_subtracted(const Vector3 *v0, const Vector3 *v1);
Vector3 get_normalized(const Vector3 *original_vector);
Vector3 get_cross_product(const Vector3 *v0, const Vector3 *v1);

float get_dot_product(const Vector3 *v0, const Vector3 *v1);

Quaternion get_multiplied_q(const Quaternion *q, const Quaternion *s);
Matrix4x4 get_multiplied_m(const Matrix4x4 *m0, const Matrix4x4 *m1);

Matrix4x4 get_transform(const Quaternion *q);

Quaternion get_quaternion(const float angle, const Vector3 *axis);

#endif