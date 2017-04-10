#include "math3d.h"


void update_perspective_projection_matrix(
	Matrix4x4 *proj,

	const float vertical_fov,
	const float aspect,
	const float z_near,
	const float z_far
) {
	float t = tanf(vertical_fov / 2.0f);

	proj->data[ 0] = 1.0f / (aspect * t);
	proj->data[ 5] = 1.0f / t;
	proj->data[10] = z_far / (z_near - z_far);
	proj->data[11] = -1.0f;
	proj->data[14] = -(z_far * z_near) / (z_far - z_near);
}

void update_view_matrix(Matrix4x4 *result, const Vector3 *eye, const Vector3 *look_at, const Vector3 *up)
{
	const Vector3 sub = get_subtracted(look_at, eye);

	Vector3 f = get_normalized(&sub);

	const Vector3 cross = get_cross_product(&f, up);

	Vector3 s = get_normalized(&cross);
	Vector3 u = get_cross_product(&s, &f);

	result->data[ 0] = s.x;
	result->data[ 4] = s.y;
	result->data[ 8] = s.z;

	result->data[ 1] = u.x;
	result->data[ 5] = u.y;
	result->data[ 9] = u.z;

	result->data[ 2] = -f.x;
	result->data[ 6] = -f.y;
	result->data[10] = -f.z;

	result->data[12] = -get_dot_product(&s, eye);
	result->data[13] = -get_dot_product(&u, eye);
	result->data[14] = get_dot_product(&f, eye);
}

Vector3 get_subtracted(const Vector3 *v0, const Vector3 *v1)
{
	Vector3 result;

	result.x = v0->x - v1->x;
	result.y = v0->y - v1->y;
	result.z = v0->z - v1->z;

	return result;
}

Vector3 get_normalized(const Vector3 *original_vector)
{
	Vector3 result;

	float original_vector_length = sqrtf(get_dot_product(original_vector, original_vector));

	result.x = original_vector->x / original_vector_length;
	result.y = original_vector->y / original_vector_length;
	result.z = original_vector->z / original_vector_length;

	return result;
}

Vector3 get_cross_product(const Vector3 *v0, const Vector3 *v1)
{
	Vector3 result;

	result.x = v0->y * v1->z - v1->y * v0->z;
	result.y = v0->z * v1->x - v1->z * v0->x;
	result.z = v0->x * v1->y - v1->x * v0->y;

	return result;
}

float get_dot_product(const Vector3 *v0, const Vector3 *v1)
{
	float result = v0->x * v1->x + v0->y * v1->y + v0->z * v1->z;

	return result;
}

Quaternion get_multiplied_q(const Quaternion *p, const Quaternion *q)
{
	Quaternion result;

	result.w = p->w * q->w - p->x * q->x - p->y * q->y - p->z * q->z;
	result.x = p->w * q->x + p->x * q->w + p->y * q->z - p->z * q->y;
	result.y = p->w * q->y + p->y * q->w + p->z * q->x - p->x * q->z;
	result.z = p->w * q->z + p->z * q->w + p->x * q->y - p->y * q->x;

	return result;
}
Matrix4x4 get_multiplied_m(const Matrix4x4 *m0, const Matrix4x4 *m1)
{
	Matrix4x4 result;

	return result;
}

Matrix4x4 get_transform(const Quaternion * q)
{
	Matrix4x4 result;

	float qxx = q->x * q->x;
	float qyy = q->y * q->y;
	float qzz = q->z * q->z;
	float qxz = q->x * q->z;
	float qxy = q->x * q->y;
	float qyz = q->y * q->z;
	float qwx = q->w * q->x;
	float qwy = q->w * q->y;
	float qwz = q->w * q->z;

	result.data[ 0] = 1.0f - 2.0f * (qyy + qzz);
	result.data[ 1] = 2.0f * (qxy + qwz);
	result.data[ 2] = 2.0f * (qxz - qwy);
	result.data[ 3] = 0.0f;

	result.data[ 4] = 2.0f * (qxy - qwz);
	result.data[ 5] = 1.0f - 2.0f * (qxx + qzz);
	result.data[ 6] = 2.0f * (qyz + qwx);
	result.data[ 7] = 0.0f;

	result.data[ 8] = 2.0f * (qxz + qwy);
	result.data[ 9] = 2.0f * (qyz - qwx);
	result.data[10] = 1.0f - 2.0f * (qxx + qyy);
	result.data[11] = 0.0f;

	result.data[12] = 0.0f;
	result.data[13] = 0.0f;
	result.data[14] = 0.0f;
	result.data[15] = 1.0f;

	return result;
}

Quaternion get_quaternion(const float angle, const Vector3 * axis)
{
	Quaternion result;

	result.x = axis->x * sinf(angle / 2.0f);
	result.y = axis->y * sinf(angle / 2.0f);
	result.z = axis->z * sinf(angle / 2.0f);
	result.w = cosf(angle / 2.0f);

	return result;
}
