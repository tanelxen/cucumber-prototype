class Player : MonoBehaviour
{
    float mouse_speed = 8.0f;
    int health = 100;

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

        drawCrosshair();
        drawHealth();
    }

    void takeDamage(int damage)
    {
        health -= damage;
        drawPain();
    }

    private void drawCrosshair()
    {
        int width = UI::GetScreenWidth();
        int height = UI::GetScreenHeight();

        Color color(1.0f, 1.0f, 1.0f, 1.0f);

        Vector2 size = Vector2(4.0f, 4.0f);
        Vector2 pos = Vector2(
            width * 0.5f - size.x * 0.5f,
            height * 0.5f - size.y * 0.5f
        );
        
        UI::DrawRect(pos, size, color);
    }

    private void drawHealth()
    {
        //int width = UI::GetScreenWidth();
        int height = UI::GetScreenHeight();

        Color color(0.9f, 0.9f, 0.9f, 1.0f);

        Vector2 pos = Vector2(16.0f, height - 16.0f);
        
        string str = format('{}', health);
        UI::DrawText(str, pos, 1.0f, color);
    }

    private void drawPain()
    {
        int width = UI::GetScreenWidth();
        int height = UI::GetScreenHeight();

        Color color(1.0f, 0.0f, 0.0f, 0.3f);

        Vector2 size = Vector2(width, height);
        Vector2 pos(0, 0);
        
        UI::DrawRect(pos, size, color);
    }
}