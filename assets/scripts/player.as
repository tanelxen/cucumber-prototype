class Player : MonoBehaviour
{
    float move_speed = 400.0f;
    float mouse_speed = 8.0f;

    private float forwardmove = 0;
    private float rightmove = 0;
    private bool jump = false;

    TransformComponent@ headTransform;

    private MovementComponent@ motor;

    void start()
    {
        print("Player started on entity: " + GetOwner().GetName());

        @motor = GetOwner().GetMovement();
    }

    void update(float dt)
    {
        TransformComponent@ transform = GetTransform();
        if (transform == null) return;

        if (motor != null)
        {
            if (Input::GetKey(KeyCode::W)) motor.AddMovementInput(transform.GetForward(), 1.0f);
            if (Input::GetKey(KeyCode::S)) motor.AddMovementInput(transform.GetForward(), -1.0f);
            if (Input::GetKey(KeyCode::D)) motor.AddMovementInput(transform.GetRight(), 1.0f);
            if (Input::GetKey(KeyCode::A)) motor.AddMovementInput(transform.GetRight(), -1.0f);

            if (Input::GetKey(KeyCode::Space)) motor.Jump();
        }

        if (Input::GetRightMouseButton())
        {
            Input::SetCursorEnabled(false);

            float mouseX = Input::GetMouseOffsetX();
            transform.SetRotation(transform.GetRotation() - Vector3(0, mouseX * mouse_speed * dt, 0));

            if (headTransform != null)
            {
                float mouseY = Input::GetMouseOffsetY();
                headTransform.SetRotation(headTransform.GetRotation() - Vector3(mouseY * mouse_speed * dt, 0, 0));
            }
        }
        else
        {
            Input::SetCursorEnabled(true);
        }
    }
}