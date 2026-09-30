class Player : MonoBehaviour
{
    float mouse_speed = 8.0f;
    int health = 100;

    private float forwardmove = 0;
    private float rightmove = 0;
    private bool jump = false;

    TransformComponent@ headTransform;

    private MovementComponent@ motor;

    private float footstepTimer = 0.0f;
    private float footstepIntervalWalking = 0.45f;

    private array<string> footstepSounds = {
        "assets/sounds/step1.wav",
        "assets/sounds/step2.wav",
        "assets/sounds/step3.wav",
        "assets/sounds/step4.wav"
    };

    // Переменная, чтобы не играть один и тот же звук два раза подряд
    private int lastSoundIndex = -1;

    private float damageAlpha = 0;

    void start()
    {
        print("Player started on entity: " + GetOwner().GetName());

        @motor = GetOwner().GetMovement();
    }

    void update(float dt)
    {
        TransformComponent@ transform = GetTransform();
        if (transform == null) return;

        bool isMoving = false; // Флаг, идет ли игрок прямо сейчас

        if (motor != null)
        {
            if (Input::GetKey(KeyCode::W)) { motor.AddMovementInput(transform.GetForward(), 1.0f); isMoving = true; }
            if (Input::GetKey(KeyCode::S)) { motor.AddMovementInput(transform.GetForward(), -1.0f); isMoving = true; }
            if (Input::GetKey(KeyCode::D)) { motor.AddMovementInput(transform.GetRight(), 1.0f); isMoving = true; }
            if (Input::GetKey(KeyCode::A)) { motor.AddMovementInput(transform.GetRight(), -1.0f); isMoving = true; }

            if (Input::GetKey(KeyCode::Space)) motor.Jump();
        }

        // Логика таймера шагов
        if (isMoving && motor.IsGrounded()) 
        {
            footstepTimer += dt;
            if (footstepTimer >= footstepIntervalWalking)
            {
                playFootstepSound();
                footstepTimer = 0.0f;
            }
        }
        else
        {
            footstepTimer = footstepIntervalWalking; 
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

        if (damageAlpha > 0)
        {
            drawPain();
            damageAlpha = Math::max(0, damageAlpha - dt);
        }
    }

    private void playFootstepSound()
    {
        // Защита от пустого массива
        if (footstepSounds.length() == 0) return;

        int index = 0;

        // Если в пуле больше одного звука, выбираем так, чтобы он не повторял предыдущий
        if (footstepSounds.length() > 1)
        {
            do {
                index = Math::RandomInt(0, footstepSounds.length() - 1);
            } while (index == lastSoundIndex);
            
            lastSoundIndex = index;
        }

        PlaySound(footstepSounds[index]);
    }

    void takeDamage(int damage)
    {
        health -= damage;
        damageAlpha = 0.3f;
    }

    private void drawCrosshair()
    {
        UI::Rect(Color(1.0f, 1.0f, 1.0f, 1.0f))
                .Center(4, 4)
                .Draw();
    }

    private void drawHealth()
    {
        UI::Text("" + health, Color(0.63f, 0.47f, 0.25f, 1.0f))
                .BottomLeft(16, 16, 0, 0) // Текст рассчитывает размер сам, ширину/высоту ставим 0
                .Draw();
    }

    private void drawPain()
    {
        UI::Rect(Color(1.0f, 0.0f, 0.0f, damageAlpha))
                .FullScreen()
                .Draw();
    }
}